#!perl

# Add our lib path to the Perl search path.
use lib "../PerlLib/";
use TestsMod;

# Check arguments.
if (!(TestsMod::checkArgs(@ARGV))) {

    die "Usage: perl runTests.pl <testfile> [options...]\n";

}

# Check if the the first argument is the help option.
if ( $ARGV[0] eq '-h' ){
    print("runTests\n");
    print("Usage: perl runTests.pl <testfile> [options....]\n");
    print("Options:\n");
    print("\t-q\t\t\tQuiet mode.  Print non-passing tests only.\n");
    print("\t-gr\t\t\tGenerate report files.\n");
    print("\t-uw <class>\t\tUse specfied class as a wrapper class.\n");
    print("\t-classpath <path>\tUse specified path as the classpath.\n");
    print("\t-workdir <dir>\t\tUse specified dir as the working dir.\n");
    print("\t-testroot <path>\tUse specified path as the test root.\n");
    print("\t-keyword <keyword>\tRun tests with the specified keyword only.\n");
    print("\t-h\t\t\tHelp.  You're in it.\n");
    exit;
}

# Run the conformance tests.
TestsMod::runTests(@ARGV);







