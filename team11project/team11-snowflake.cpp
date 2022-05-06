// compile with: clang++ -std=c++20 -Wall -Werror -Wextra -Wpedantic -g3 -o team11-snowflake team11-snowflake.cpp
// run with: ./fishies 2> /dev/null
// run with: ./fishies 2> debugoutput.txt
//  "2>" redirect standard error (STDERR; cerr)
//  /dev/null is a "virtual file" which discard contents

// Works best in Visual Studio Code if you set:
//   Settings -> Features -> Terminal -> Local Echo Latency Threshold = -1

// https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono> // for dealing with time intervals
#include <cmath> // for max() and min()
#include <termios.h> // to control terminal modes
#include <unistd.h> // for read()
#include <fcntl.h> // to enable / disable non-blocking read()

// Because we are only using #includes from the standard, names shouldn't conflict
using namespace std;

// Constants

// Disable JUST this warning (in case students choose not to use some of these constants)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

//-------------------------------------------BUTTON CHANGES-------------------------------------------

// Due to the nature of the game we are making, some buttons such as vertical movement, were not 
// needed to be kept, and simply made reading the code more confusing, so we opted to remove some of 
// the buttons
const char NULL_CHAR     { 'z' };
const char LEFT_CHAR     { 'a' };
const char RIGHT_CHAR    { 'd' };
const char QUIT_CHAR     { 'q' };
const char CREATE_CHAR   {  }; //takes away the ability to add more buckets
const char BLOCKING_CHAR { 'b' };
const char COMMAND_CHAR  { 'o' };

const string ANSI_START { "\033[" };
const string START_COLOUR_PREFIX {"1;"};
const string START_COLOUR_SUFFIX {"m"};
const string STOP_COLOUR  {"\033[0m"};

const unsigned int COLOUR_IGNORE  { 0 }; // this is a little dangerous but should work out OK
const unsigned int COLOUR_BLACK   { 30 };
const unsigned int COLOUR_RED     { 31 };
const unsigned int COLOUR_GREEN   { 32 };
const unsigned int COLOUR_YELLOW  { 33 };
const unsigned int COLOUR_BLUE    { 34 };
const unsigned int COLOUR_MAGENTA { 35 };
const unsigned int COLOUR_CYAN    { 36 };
const unsigned int COLOUR_WHITE   { 37 };

const unsigned short MOVING_NOWHERE { 0 };
const unsigned short MOVING_LEFT    { 1 };
const unsigned short MOVING_RIGHT   { 2 };
const unsigned short MOVING_UP      { 3 };
const unsigned short MOVING_DOWN    { 4 };

#pragma clang diagnostic pop

//----------------------------------------------STRUCTS-----------------------------------------------

// We added structs for the important objects on our game - snowflake and bucket - in order to make 
// the code much more understandable and simplistic for reading and coding
struct position { int row; int col; };

struct snowflake 
{
    position position {1, 3 + (rand() % 45)};
    unsigned int colour = COLOUR_BLUE;
};

struct bucket 
{
    position position {30, 25};
    unsigned int colour = COLOUR_BLUE;
};

typedef vector< snowflake > snowflakevector;

// Globals

struct termios initialTerm;
default_random_engine generator;
uniform_int_distribution<int> movement(-1,1);
uniform_int_distribution<unsigned int> fishcolour( COLOUR_CYAN, COLOUR_WHITE );

//==================================================================================================//
//                                           NOT CHANGING                                           //
//==================================================================================================//

// Utilty Functions

// These two functions are taken from StackExchange and are 
// all of the "magic" in this code.
auto SetupScreenAndInput() -> void
{
    struct termios newTerm;
    // Load the current terminal attributes for STDIN and store them in a global
    tcgetattr(fileno(stdin), &initialTerm);
    newTerm = initialTerm;
    // Mask out terminal echo and enable "noncanonical mode"
    // " ... input is available immediately (without the user having to type 
    // a line-delimiter character), no input processing is performed ..."
    newTerm.c_lflag &= ~ICANON;
    newTerm.c_lflag &= ~ECHO;
    newTerm.c_cc[VMIN] = 1;
 
    // Set the terminal attributes for STDIN immediately
    auto result { tcsetattr(fileno(stdin), TCSANOW, &newTerm) };
    if ( result < 0 ) { cerr << "Error setting terminal attributes [" << result << "]" << endl; }
}
auto TeardownScreenAndInput() -> void
{
    // Reset STDIO to its original settings
    tcsetattr( fileno( stdin ), TCSANOW, &initialTerm );
}
auto SetNonblockingReadState( bool desiredState = true ) -> void
{
    auto currentFlags { fcntl( 0, F_GETFL ) };
    if ( desiredState ) { fcntl( 0, F_SETFL, ( currentFlags | O_NONBLOCK ) ); }
    else { fcntl( 0, F_SETFL, ( currentFlags & ( ~O_NONBLOCK ) ) ); }
    // cerr << "SetNonblockingReadState [" << desiredState << "]" << endl;
}
// Everything from here on is based on ANSI codes
// Note the use of "flush" after every write to ensure the screen updates
auto ClearScreen() -> void { cout << ANSI_START << "2J" << flush; }
auto MoveTo( unsigned int x, unsigned int y ) -> void { cout << ANSI_START << x << ";" << y << "H" << flush; }
auto HideCursor() -> void { cout << ANSI_START << "?25l" << flush; }
auto ShowCursor() -> void { cout << ANSI_START << "?25h" << flush; }
auto GetTerminalSize() -> position
{
    // This feels sketchy but is actually about the only way to make this work
    MoveTo(999,999);
    cout << ANSI_START << "6n" << flush ;
    string responseString;
    char currentChar { static_cast<char>( getchar() ) };
    while ( currentChar != 'R')
    {
        responseString += currentChar;
        currentChar = getchar();
    }
    // format is ESC[nnn;mmm ... so remove the first 2 characters + split on ; + convert to unsigned int
    // cerr << responseString << endl;
    responseString.erase(0,2);
    // cerr << responseString << endl;
    auto semicolonLocation = responseString.find(";");
    // cerr << "[" << semicolonLocation << "]" << endl;
    auto rowsString { responseString.substr( 0, semicolonLocation ) };
    auto colsString { responseString.substr( ( semicolonLocation + 1 ), responseString.size() ) };
    // cerr << "[" << rowsString << "][" << colsString << "]" << endl;
    auto rows = stoul( rowsString );
    auto cols = stoul( colsString );
    position returnSize { static_cast<int>(rows), static_cast<int>(cols) };
    // cerr << "[" << returnSize.row << "," << returnSize.col << "]" << endl;
    return returnSize;
}
auto MakeColour( string inputString, 
                 const unsigned int foregroundColour = COLOUR_WHITE,
                 const unsigned int backgroundColour = COLOUR_IGNORE ) -> string
{
    string outputString;
    outputString += ANSI_START;
    outputString += START_COLOUR_PREFIX;
    outputString += to_string( foregroundColour );
    if ( backgroundColour ) 
    { 
        outputString += ";";
        outputString += to_string( ( backgroundColour + 10 ) ); // Tacky but works
    }
    outputString += START_COLOUR_SUFFIX;
    outputString += inputString;
    outputString += STOP_COLOUR;
    return outputString;
}

//==================================================================================================//
//                                             OUR CODE                                             //
//==================================================================================================//

//-------------------------------------------UI SYSTEM----------------------------------------------
unsigned int points {0};
unsigned int lives {3};

// Both the ScoreIncrease and LiveDecrease methods are very simple, and are not totally necessary 
// because they increase the number of lines, but we included them because they make the code more 
// readable for users
auto ScoreIncrease() -> void
{
    points++;
}

auto LiveDecrease() -> void
{
    lives--;
}

//-------------------------------------------Diplay UI----------------------------------------------

// Like the Score and Live change methods, we could combine the DrawScore and DrawLives method, 
// However the code is much more understandable when the methods are seperated
auto DrawScore() -> void
{
    MoveTo( 1, 1 ); 
    cout << "Points: " << points << flush;
}

auto DrawLives() -> void
{
    MoveTo( 2, 1);
    cout << "Lives: " << lives << flush;
}

auto DrawLoseScreen() -> void
{
    string red = "\033[1;31m";
    string white = "\033[0m\n";

    cout << red << endl;
    cout << " ██████╗  █████╗ ███╗   ███╗███████╗   █████╗ ██╗   ██╗███████╗██████╗ " << endl;
    cout << "██╔════╝ ██╔══██╗████╗ ████║██╔════╝  ██╔══██╗██║   ██║██╔════╝██╔══██╗" << endl;
    cout << "██║  ██╗ ███████║██╔████╔██║█████╗    ██║  ██║╚██╗ ██╔╝█████╗  ██████╔╝" << endl;
    cout << "██║  ╚██╗██╔══██║██║╚██╔╝██║██╔══╝    ██║  ██║ ╚████╔╝ ██╔══╝  ██╔══██╗" << endl;
    cout << "╚██████╔╝██║  ██║██║ ╚═╝ ██║███████╗  ╚█████╔╝  ╚██╔╝  ███████╗██║  ██║" << endl;
    cout << " ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝   ╚════╝    ╚═╝   ╚══════╝╚═╝  ╚═╝" << endl;
    cout << endl;
    cout << white <<"                              Score: " << points << endl;
}

//-------------------------------------------Check Lives---------------------------------------------

// An important step of the game is checking the live count, this method was created to keep 
// simplicity in the main method
auto CheckLives() -> bool
{
    if (lives == 0)
    {
        //When the number of lifes becomes 0, this means that the user has lost all their lifes
        //and by returning true, this tells the program that the player has died and the game must end
        return true;
    }
    return false;
}

//-------------------------------------------Bucket Logic---------------------------------------------

// Utilizing a method for bucket horizontal movement by updating the former fish movement code to 
// better fit our game
auto UpdateBucketPosition( bucket & theBucket, char currentChar ) -> void
{
    // Deal with movement commands
    int commandColChange = 0;

    //check the direction of column change
    if ( currentChar == LEFT_CHAR )  { commandColChange -= 1; }
    if ( currentChar == RIGHT_CHAR ) { commandColChange += 1; }

    // Update the column of bucket
    auto currentColChange { commandColChange };
    auto proposedCol { theBucket.position.col + currentColChange };
    theBucket.position.col = max(  1, min( 40, proposedCol ) );
    
}

// We could have likely combined all of the drawing methods but having seperate drawing methods for each
// important component of our game in the main method makes it more understandable
auto DrawBucket(bucket theBucket) -> void
{
    //draw the buckets initial position
    MoveTo( theBucket.position.row, theBucket.position.col ); 
    cout << MakeColour("\\       /", theBucket.colour) << flush;
    MoveTo( theBucket.position.row + 1, theBucket.position.col ); 
    cout << MakeColour( " \\_____/ ", theBucket.colour ) << flush; 
}


//-------------------------------------------Snoflake Logic-------------------------------------------

//Make the snowflake spawn at the random position of the top row and add it to the snowflake vector
auto CreateSnowflake(snowflakevector & snowflakes) -> void
{
    cerr << "creating Snowflake" << endl;
    snowflake newSnowflake {
        .position = {1, 3 + (rand() % 45)},
        .colour = COLOUR_BLUE,
    };
    snowflakes.push_back(newSnowflake);
}

// Like stated in for the DrawBucket method, having a specific method for drawing snowflakes makes
// changing the code easy
auto DrawSnowflakes(snowflakevector snowflakes) -> void
{
    for (auto currSnowflake : snowflakes)
    {
        MoveTo( currSnowflake.position.row, currSnowflake.position.col ); 
        cout << MakeColour("❄", currSnowflake.colour) << flush;
    }
}

// Using a method just for gravity on the snowflakes on the screen sections the most difficult parts
// of the code into easy smaller sections
auto GravityOnSnowflake(snowflakevector & snowflakes) -> void
{
    for (auto & currSnowflake : snowflakes)
    {
        
        if (currSnowflake.position.row <= 30)
        {
            currSnowflake.position.row += 1;
        }
    }
}

// (a) Check if all current snowflakes on the screen are being caught by the buecket - if so, increase score
//     if not check if the player should lose a live, then update the current snowflake vector to only contain
//     snowflakes still on the screen
// (b) The most complex and important part of the game, we focused on producing a method which mainly would 
//     determine if score or lives should change, while updating the snowflake vector to keep the game readable
auto IntersectionWithSnowflake(snowflakevector & snowflakes, bucket theBucket) -> void
{
    snowflakevector newvec;
    for (auto & currSnowflake : snowflakes)
    {
        if (currSnowflake.position.row == 29 
        and abs(currSnowflake.position.col - theBucket.position.col - 4) <= 4 )
        {
            ScoreIncrease();
        }
        else if (currSnowflake.position.row <= 30)
        {
            if (currSnowflake.position.row == 30)
            {
                LiveDecrease();
            }
            newvec.push_back(currSnowflake);
        }
    }
    snowflakes = newvec;
}


//-------------------------------------------Main Method----------------------------------------------

auto main() -> int
{
    // Set Up the system to receive input
    SetupScreenAndInput();

    // Check that the terminal size is large enough for our fishies
    const position TERMINAL_SIZE { GetTerminalSize() };
    if ( ( TERMINAL_SIZE.row < 30 ) or ( TERMINAL_SIZE.col < 50 ) )
    {
        ShowCursor();
        TeardownScreenAndInput();
        cout << endl <<  "Terminal window must be at least 30 by 50 to run this game" << endl;
        return EXIT_FAILURE;
    }

    // State Variables
    snowflakevector snowflakes;
    unsigned int ticks {0};

    char currentChar { CREATE_CHAR }; // the first act will be to create the bucket
    string currentCommand;

    bucket theBucket {
        .position = { .row = 30, .col = 25 } ,
        .colour = COLOUR_BLUE, 
    };

    bool allowBackgroundProcessing { true };
    bool showCommandline { false };

    auto startTimestamp { chrono::steady_clock::now() };
    auto endTimestamp { startTimestamp };
    int elapsedTimePerTick { 100 }; // Every 0.1s check on things
    
    SetNonblockingReadState( allowBackgroundProcessing );
    ClearScreen();
    HideCursor();

    while( currentChar != QUIT_CHAR )
    {
        endTimestamp = chrono::steady_clock::now();
        auto elapsed { chrono::duration_cast<chrono::milliseconds>( endTimestamp - startTimestamp ).count() };
        // We want to process input and update the world when EITHER  
        // (a) there is background processing and enough time has elapsed
        // (b) when we are not allowing background processing.
        if ( 
                 ( allowBackgroundProcessing and ( elapsed >= elapsedTimePerTick ) )
              or ( not allowBackgroundProcessing ) 
           )
        {
            ticks++;
            cerr << "Ticks [" << ticks << "] allowBackgroundProcessing ["<< allowBackgroundProcessing << "] elapsed [" << elapsed << "] currentChar [" << currentChar << "] currentCommand [" << currentCommand << "]" << endl;
            if ( currentChar == BLOCKING_CHAR ) // Toggle background processing
            {
                allowBackgroundProcessing = not allowBackgroundProcessing;
                SetNonblockingReadState( allowBackgroundProcessing );
            }
            
            // all bucket and snowflake methods being used
            UpdateBucketPosition( theBucket, currentChar );
            
            // reduce the speed of gravity
            if (ticks % 5 == 0)
            {
                GravityOnSnowflake(snowflakes);
                IntersectionWithSnowflake(snowflakes, theBucket);
            }
            
            // reduce the rate of snowflakes spawning
            if (ticks % 25 == 0)
            {
                CreateSnowflake(snowflakes);
            }
            
            // Drawing all interface components
            ClearScreen();
            DrawBucket( theBucket );
            DrawSnowflakes( snowflakes );
            DrawScore();
            DrawLives();

            // Check if lives are not 0
            if (CheckLives())
            {
                break;
            }

            // Clear inputs in preparation for the next iteration
            startTimestamp = endTimestamp;    
            currentChar = NULL_CHAR;
            currentCommand.clear();
        }
        // Depending on the blocking mode, either read in one character or a string (character by character)
        if ( showCommandline )
        {
            while ( read( 0, &currentChar, 1 ) == 1 && ( currentChar != '\n' ) )
            {
                cout << currentChar << flush; // the flush is important since we are in non-echoing mode
                currentCommand += currentChar;
            }
            cerr << "Received command [" << currentCommand << "]" << endl;
            currentChar = NULL_CHAR;
        }
        else
        {
            read( 0, &currentChar, 1 );
        }
    }

    // Game over Screen
    ClearScreen();
    DrawLoseScreen();

    // Tidy Up and Close Down
    ShowCursor();
    SetNonblockingReadState( false );
    TeardownScreenAndInput();
    cout << endl; // be nice to the next command
    return EXIT_SUCCESS;
}
