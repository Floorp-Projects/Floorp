#!perl
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

package TestsMod;
use Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(runTests,checkArgs);

#
# runSuite
#
# Runs each test package it reads from the given file.
#
sub runSuite {

    my ($testsuite) = @_;

    print "Running test suite: $testsuite\n";

    # Open the test suite and run each test package read.
    open TESTSUITE, $testsuite || die ("Cannot open testsuite.\n");

    while ($inputString = <TESTSUITE>) {

	# Parse the string for the testpackage and if there is a wrapper class.
	($testPackage, $wrapperClass) = ($inputString =~ /^(\S+)\s+(.*)/);
	
	if ($wrapperClass =~ /[A-Za-z]/) { 
	    @args = ("${testPackage}.txt", "-q", "-gt", "-uw", 
		     "$wrapperClass");
	} else {
	    @args = ("${testPackage}.txt", "-q", "-gt");
	}
	runTests(@args);
    
	# Send the report to the server.
	ReportMod::sendReport("ranma","ranma","8765","report.txt");

	# Backup the current report.
	rename("report.txt", "${testPackage}Report.old");

    }

    close TESTSUITE;

}

#
# runTests
#
# Performs the conformance tests on a given java implementation.
#
#
sub runTests {

    my (@args) = @_;

    # Global variables.
    $testfile = "";
    $useWorkDir = 0;
    $workDir = "";
    $useTestRoot = 0;
    $testRoot = "";
    $genReport = 0;
    $genResult = 0;
    $useWrapper = 0;
    $wrapperClass = "";
    $useDots = 0;
    $classpath = $ENV{CLASSPATH};

    # Test info.
    $testID = "";
    $executeClass = "";
    $executeArgs = "";
    $result = "";
    $resultLog = "";
    $color = "";
 
    # Width count.
    $widthCount = 0;

    # Result counters.
    $totalTests = 0;
    $totalPassed = 0;
    $totalFailed = 0;
    $totalException = 0;
    $totalAssert = 0;
    $totalCheckTest = 0;
    
    # Parse the argument list.
    &parseArgs(@args);

    print("Test file: $testfile\n");
    print("Working dir: $workDir\n");
    print("Test root: $testDir\n");
    print("Generate results: $genResult\n");
    print("Wrapper class: $wrapperClass\n");
    print("Classpath: $classpath\n");
    print("\n");

    # Open the test file.
    open TESTLIST, $testfile || die ("Cannot open test file.\n");

    # Open the html result files.
    if ($genResult) {

	open RESULTS, "+>report.html" || die ("Cannot open report file.\n");
	open PASSED, "+>passed.html" || die ("Cannot open passed file.\n");
	open FAILED, "+>failed.html" || die ("Cannot open failed file.\n");
	open EXCEPTION, "+>exception.html" || die ("Cannot open exception file.\n");
	open ASSERT, "+>assert.html" || die ("Cannot open assert file.\n");
	open CHECKTEST, "+>checktest.html" || die ("Cannot open check test file.\n");
	
         # Generate the headers for the result html.
	&genResultHeader;

    }

    # Open the report file.
    if ($genReport) {
	open REPORT, "+>report.txt" || die ("Cannot open results file.\n");
    }

    while ($testID = <TESTLIST>) {

	# ***** Need to take care of blanks, space, etc... ***** #

	$testString = <TESTLIST>;

        # Parse the test string for the execute class and arguments.
	($executeClass, $executeArgs) = ($testString =~ /^(\S+)\s+(.*)/);

	&resolveArgs;

	# Excute the test.
	$totalTests++;
	if ($useWrapper) {
	    $executeString = "sajava -classpath $classpath -sys -ce $wrapperClass $executeClass $executeArgs";
	} else {
	    $executeString = "sajava -classpath $classpath -sys -ce $executeClass $executeArgs";
	}

	open(LOG,"$executeString 2>&1 |") || die ("Could not execute test\n");

	while(<LOG>) {
	    $resultLog .= $_;
	}
	close LOG;
	#print("Result: $resultLog\n");

	# Parse the result string.  Print to stdout and report.
	$loweredResult = lc $resultLog;
	if ($loweredResult =~ /status:passed./) {
	    
	    $totalPassed++;
	    $result = "PASSED";
	    $color = "#99FF99";
	    
	    if ($useDots) {
		&printDot;
	    } else {
		&printToScreen;
	    }
	    
	    if ($genResult) {
		&genTestResult(*PASSED);
	    }
	} 
	
	elsif ($loweredResult =~ /status:failed./) {
	    
	    $totalFailed++;
	    $result = "FAILED";
	    $color = "#FF6666";
	    &printToScreen;
	    
	    if ($genResult) {
		&genTestResult(*FAILED);
	    }
	} 
	
	elsif ($loweredResult =~ /status:not run./) {
	    
	    $totalFailed++;
	    $result = "FAILED";
	    $color = "#FF6666";
	    &printToScreen;
	    
	    if ($genResult) {
		&genTestResult(*FAILED);
	    }
	} 
	
	elsif ($loweredResult =~ /status:exception./) {
	    
	    $totalException++;
	    $result = "EXCEPTION";
	    $color = "#FF6666";
	    &printToScreen;
	    
	    if ($genResult) {
		&genTestResult(*EXCEPTION);
	    }
	}
	
	elsif ($loweredResult =~ /status:assertion./) {
	    
	    $totalAssert++;
	    $result = "ASSERTION";
	    $color = "#FF6666";
	    &printToScreen;
	    
	    if ($genResult) {
		&genTestResult(*ASSERT);
	    }
	}
	
	else {
	    
	    $totalCheckTest++;
	    $result = "CHECK TEST";
	    $color = "#8080FF";
	    &printToScreen;
	    
	    if ($genResult) {
		&genTestResult(*CHECKTEST);
	    }
	}

	# Print the result to the report file.
	if ($genReport) {
	    &printTestReport(*REPORT);
	}
	
	#Clear the test info.
	$testID = "";
	$executeClass = "";
	$executeArgs = "";
	$result = "";
	$resultLog = "";
	$color = "";
	
    }
    
    print("\nDone with tests.\n");
    &genResultFooter;
    
    # Close files.
    close TESTLIST;

    # Close the report files.
    if ($genResult) {
	
	close RESULTS;
	close PASSED;
	close FAILED;
	close EXCEPTION;
	close ASSERT;
	close CHECKTEST;
	
    }

    # Close the result file.
    if ($genReport) {
	close REPORT;
    }
    
}

#
# checkArgs
#
# Checks to see if there are command line arguments.
# Returns true(1) or false(0).
#
sub checkArgs {

    local @args = @_;

    # print("Number of args: $#args\n");

    if ($#args == -1) {
	return 0;
    }

    return 1;

}

#
# parseArgs
#
# Go through the argument list and set the matching global
# variables.
#
sub parseArgs {

    local @args = @_;

    # Check if the first argument is the help option.
    if ( $args[$i] eq '-h' ){
	print("runTests\n");
	print("Usage: runTests <testfile> [options....]\n");
	print("Options:\n");
	print("\t-q\t\t\tQuiet mode.  Print non-passing tests only.\n");
	print("\t-gr\t\t\tGenerate result files in html format.\n");
	print("\t-gt\t\t\tGenerate a report file in text format.\n");
	print("\t-uw <class>\t\tUse specfied class as a wrapper class.\n");
	print("\t-classpath <path>\tUse specified path as the classpath.\n");
	print("\t-workdir <dir>\t\tUse specified dir as the working dir.\n");
	print("\t-testroot <path>\tUse specified path as the test root.\n");
	print("\t-keyword <keyword>\tRun tests with the specified keyword only.\n");
	print("\t-h\t\t\tHelp.  You're in it.\n");
	exit;
    }

    # The first argument should the test file.
    $testfile = $args[0];

    # Check if the file exits.
    if (!(-e $testfile)) {
	die "Test file does not exist in the current directory.\n";
    }

    # Go through the rest of the arguments.
    print("Args: ");
    $i = 0;
    while( $i < @args ){

	#print("$args[$i]", "\n");

        if ( $args[$i] eq '-gr' ){
            $genResult = 1;
        }

        if ( $args[$i] eq '-gt' ){
            $genReport = 1;
        }

        if ( $args[$i] eq '-q' ){
            $useDots = 1;
        }

	elsif ( $args[$i] eq '-uw' ) {
	    $useWrapper = 1;
	    $i++;
	    $wrapperClass = $args[$i];

	    # Check if a wrapper class has been given.
	    if ( $wrapperClass eq '' ) {
		die ("No wrapper class specified.\n");
	    }
	}

	elsif ( $args[$i] eq '-classpath' ) {
	    $i++;
	    $classpath = $args[$i];

	    #Check if the given work dir is valid.
	    if ( $classpath eq '' ) {
		die ("No classpath specified.\n");
	    } 
	}

	elsif ( $args[$i] eq '-workdir' ) {
	    $useWorkDir = 1;
	    $i++;
	    $workDir = $args[$i];

	    #Check if the given work dir is valid.
	    if ( $workDir eq '' ) {
		die ("No working directory specified.\n");
	    } elsif (!(-e $workDir)) {
		die ("Working directory does not exist.\n");
	    } else {
		$workDir .= "/";
	    }
	}

	elsif ( $args[$i] eq '-testroot' ) {
	    $useTestRoot = 1;
	    $i++;
	    $testRoot = $args[$i];

	    #Check if the given test root is valid.
	    if ( $testRoot eq '' ) {
		die ("No test root specified.\n");
	    } 
	}

	$i++;

	# Ignore the rest of the arguments.

    }
    print("\n");

    return;
    
}

#
# resolveArgs
#
# Replace any test argument with the correct value.
#
sub resolveArgs {

    if ($useWorkDir) {
	$executeArgs =~ s/-WorkDir\s+(\S+)/-WorkDir $workDir/;
	$executeArgs =~ s/-workDir\s+(\S+)/-WorkDir $workDir /;
	$executeArgs =~ s/-TestWorkDir\s+(\S+)/-WorkDir $workDir /;
    } else {
	# Remove the argument name and value from the string.
	$executeArgs =~ s/-WorkDir\s+(\S+)/\ /;
	$executeArgs =~ s/-workDir\s+(\S+)/\ /;
	$executeArgs =~ s/-TestWorkDir\s+(\S+)/\ /;
    }

    if ($useTestRoot) {
	$executeArgs =~ s/file:\/G:\/JCK-114a/$testRoot/;
    } else {
	# Remove the argument name and value from the string.
	$executeArgs =~ s/-Test\s+(\S+)/\ /;	
	$executeArgs =~ s/-test\s+(\S+)/\ /;
	$executeArgs =~ s/-TestURL\s+(\S+)/\ /;
	$executeArgs =~ s/-testURL\s+(\S+)/\ /;
    }

    return;

}

#
# printDot
# 
# Prints a dot on stdout.  
#
sub printDot {

    # Check if width exceeds 80 chars.
    if ($widthCount >= 80) {
	
	print("\n");
	print(".");

	$widthCount = 0;

    } else {

	$widthCount++;
	print(".");

    }
	
    return;

}

#
# printToScreen
#
# Prints the test result to stdout.
#
sub printToScreen {

    print("\nTestCaseID: $testID");
    print("Execute Class: $executeClass\n");
    print("Execute Args: $executeArgs\n");
    print("Result: $result.\n");
    
    if (!($result eq 'PASSED')) {
	print("Result Log:\n");
	print("$resultLog\n");
	$widthCount = 0;
    } 

}

#
# genResultHeader
#
# Prints out headers for the result files.
#
sub genResultHeader {

    print RESULTS "<HTML>\n";
    print RESULTS "<TITLE>Test Results</TITLE>\n";
    print RESULTS "<BODY>\n";

    print PASSED "<HTML>\n";
    print PASSED "<TITLE>Pass Results</TITLE>\n";
    print PASSED "<BODY>\n";

    print FAILED "<HTML>\n";
    print FAILED "<TITLE>Fail Results</TITLE>\n";
    print FAILED "<BODY>\n";

    print EXCEPTION "<HTML>\n";
    print EXCEPTION "<TITLE>Exceptions Results</TITLE>\n";
    print EXCEPTION "<BODY>\n";

    print ASSERT "<HTML>\n";
    print ASSERT "<TITLE>Assert Results</TITLE>\n";
    print ASSERT "<BODY>\n";

    print CHECKTEST "<HTML>\n";
    print CHECKTEST "<TITLE>Check Test Results</TITLE>\n";
    print CHECKTEST "<BODY>\n";

}

#
# genResultFooter
#
# Prints out footers for the result files.
#
sub genResultFooter {

    # Print out totals to the result file.
    &genResults;
    print RESULTS "</BODY>\n";
    print RESULTS "</HTML>\n";

    print PASSED "</BODY>\n";
    print PASSED "</HTML>\n";

    print FAILED "</BODY>\n";
    print FAILED "</HTML>\n";

    print EXCEPTION "</BODY>\n";
    print EXCEPTION "</HTML>\n";

    print ASSERT "</BODY>\n";
    print ASSERT "</HTML>\n";

    print CHECKTEST "</BODY>\n";
    print CHECKTEST "</HTML>\n";

}

#
# genResults
#
# Print out the totals for the test run.
#
sub genResults {

    #Print out the totals.
    print RESULTS "<TABLE CELLPADDING=2 WIDTH=\"350\">\n";

    print RESULTS "</TABLE><P>\n";
    print RESULTS "<TABLE CELLPADDING=2 WIDTH=\"350\">\n";

    print RESULTS "<TR>\n";
    print RESULTS "<TD WIDTH=\"50\" BGCOLOR=\"#D4D4D4\"><B>Total</B></TD>\n";

    print RESULTS "<TD BGCOLOR=\"#E4E4E4\" ALIGN=\"CENTER\"><B>\n";
    print RESULTS $totalTests;
    print RESULTS "</B></FONT></TD>\n";
    print RESULTS "</TR>\n";
    
    print RESULTS "<TR>\n";
    print RESULTS "<TD WIDTH=\"50\" BGCOLOR=\"#D4D4D4\"><A HREF=\"\n";
    print RESULTS "passed.html\"><B>Passed</B></A></TD>\n";

    print RESULTS "<TD BGCOLOR=\"#99FF99\" ALIGN=\"CENTER\"><B>\n";
    print RESULTS $totalPassed; 
    print RESULTS "</B></TD>\n";
    print RESULTS "</TR>\n";

    print RESULTS "<TR>\n";
    print RESULTS "<TD WIDTH=\"50\" BGCOLOR=\"#D4D4D4\"><A HREF=\"\n";
    print RESULTS "failed.html\"><B>Failed</B></A></TD>\n";

    print RESULTS "<TD BGCOLOR=\"#FF6666\" ALIGN=\"CENTER\"><B>\n";
    print RESULTS $totalFailed;
    print RESULTS "</B></TD>\n";
    print RESULTS "</TR>\n";

    print RESULTS "<TR>\n";
    print RESULTS "<TD WIDTH=\"50\" BGCOLOR=\"#D4D4D4\"><A HREF=\"\n";
    print RESULTS "exception.html\"><B>Unhandled Exceptions</B></A></TD>\n"; 

    print RESULTS "<TD BGCOLOR=\"#FF6666\" ALIGN=\"CENTER\"><B>\n";
    print RESULTS $totalException;
    print RESULTS "</B></TD>\n";
    print RESULTS "</TR>\n";

    print RESULTS "<TR>\n";
    print RESULTS "<TD WIDTH=\"50\" BGCOLOR=\"#D4D4D4\"><A HREF=\"\n";
    print RESULTS "assert.html\"><B>Assertions Thrown</B></A></TD>\n"; 

    print RESULTS "<TD BGCOLOR=\"#FF6666\" ALIGN=\"CENTER\"><B>";
    print RESULTS $totalAssert;
    print RESULTS "</B></TD>\n";
    print RESULTS "</TR>\n";

    print RESULTS "<TR>\n";
    print RESULTS "<TD WIDTH=\"50\" BGCOLOR=\"#D4D4D4\"><A HREF=\"\n";
    print RESULTS "checktest.html\"><B>Check Test</B></A></TD>\n";

    print RESULTS "<TD BGCOLOR=\"#8080FF\" ALIGN=\"CENTER\"><B>\n";
    print RESULTS $totalCheckTest;
    print RESULTS "</B></TD>\n";
    print RESULTS "</TR>\n";

    print RESULTS "</TABLE><P>\n";

}

#
# genTestResult
#
# Print out the test to the right result file.
#
sub genTestResult {

    local(*RESULTFILE) = @_;

    print RESULTFILE "<TABLE CELLPADDING=2 WIDTH=\"100%\">\n";

    print RESULTFILE "<TR>\n";
    print RESULTFILE "<TD WIDTH=\"10%\" BGCOLOR=\"#D4D4D4\"><B>Test ID</B></TD>\n";

    print RESULTFILE "<TD BGCOLOR=\"#E4E4E4\"><B>";
    print RESULTFILE $testID;
    print RESULTFILE "</B></FONT></TD>\n";
    print RESULTFILE "</TR>\n";

    print RESULTFILE "<TR>\n";
    print RESULTFILE "<TD WIDTH=\"10%\" BGCOLOR=\"#D4D4D4\"><B>Execute Class</B></TD>\n";

    print RESULTFILE "<TD BGCOLOR=\"#E4E4E4\"><B>";
    print RESULTFILE $executeClass;
    print RESULTFILE "</B></FONT></TD>\n";
    print RESULTFILE "</TR>\n";
    
    print RESULTFILE "<TR>\n";
    print RESULTFILE "<TD WIDTH=\"10%\" BGCOLOR=\"#D4D4D4\"><B>Execute Arguments</B></TD>\n";

    print RESULTFILE "<TD BGCOLOR=\"#E4E4E4\"><B>";
    print RESULTFILE "${executeArgs}&nbsp;"; 
    print RESULTFILE "</B></FONT></TD>\n";
    print RESULTFILE "</TR>\n";
    
    print RESULTFILE "<TR>\n";
    print RESULTFILE "<TD WIDTH=\"10%\" BGCOLOR=\"#D4D4D4\"><B>Result</B></TD>\n";
    
    print RESULTFILE "<TD BGCOLOR=\"";
    print RESULTFILE $color;
    print RESULTFILE "\"><B>";
    print RESULTFILE $result;
    print RESULTFILE "</B></TD>\n";
    print RESULTFILE "</TR>\n";

    #Only print the log if the test didn't pass.
    if (!($result eq 'PASSED')) {

	print RESULTFILE "<TR>\n";
	print RESULTFILE "<TD WIDTH=\"10%\" BGCOLOR=\"#D4D4D4\"><B>Log</B></TD>\n";
	
	print RESULTFILE "<TD BGCOLOR=\"#E4E4E4\"><PRE>\n";
	print RESULTFILE $resultLog; 
	print RESULTFILE "</PRE></TD>\n";
	print RESULTFILE "</TR>\n";

    }    

    print RESULTFILE  "</TABLE><P>\n";

}

#
# printTestReport
#
# Print out the test result to the report file.
#
sub printTestReport {

    local(*REPORTFILE) = @_;

    print REPORTFILE "$testID";
    #Check for new line.
    if ($executeArgs =~ /\n/) {
	print REPORTFILE "$executeClass $executeArgs";
    } else {
	print REPORTFILE "$executeClass $executeArgs\n";
    }
    print REPORTFILE "$result\n";

    # If the test didn't passed, print out the result log.
    if (!($result eq 'PASSED')) {
	print REPORTFILE "<BEGIN TEST LOG>\n";
	print REPORTFILE "$resultLog";
	print REPORTFILE "<END TEST LOG>\n";
    }

}

