# !/bin/perl
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s):

###################################################################
# This script is used to invoke automated test cases for Pluglets
# and to record their results
#

# Attach Perl Libraries
###################################################################
use Cwd;
use Cwd 'abs_path';
use File::Copy;
#For waitpid
use POSIX ":sys_wait_h";

###################################################################


# delimiter for logfile
$delimiter="###################################################\n";

# win32 specific values 
# STILL_ALIVE is a constant defined in winbase.h and indicates what
# process still alive
if ($^O=~/Win32/) {
    $STILL_ALIVE = 0x103;
    $path_separator = ";";
    $file_separator = "\\";
    $program_args = "-P mozProfile"; #Used in debug builds only
    $program_name ="mozilla.exe";
} else  { #Unix systems
    $path_separator = ":";
    $file_separator = "\/";
    $program_name ="mozilla-bin";
    $program_args = "-P mozProfile"; #Used in debug builds only
} 

##################################################################
# Usage
##################################################################
sub usage() {

    print "\n";
    print "##################################################################\n";
    print "   perl autorun.pl [ -f <test cases list file> ]\n";
    print "\n";
    print " where <test cases list file> is file with test cases list\n";
    print "\n";
    print "or:\n";
    print "   perl autorun.pl [ -t <test case> ]\n";
    print "\n";
    print " where <test case> is one of the test cases IDs\n";
    print " name ex: basic/api/PlugletTagInfo2_getAttributes\n";
    print "\n";
    print "##################################################################\n";
    print "\n";
   
}

##################################################################
# Title display
##################################################################
sub title() {
	
    print "\n";
    print "##################################################################\n";
    print "        Automated Execution of Java Pluglet API TestSuite\n";
    print "##################################################################\n";
    print "\n";
    print "\n";
}

##################################################################
#
# 
#
##################################################################
sub checkRun() {
    $runtype = "1";
}


#########################################################################
#
# Construct Output HTML file Header
#
#########################################################################
sub constructHTMLHeader() {
   print LOGHTML "<html><head><title>\n";
   print LOGHTML "Pluglet API Test Status\n";
   print LOGHTML "</title></head><body bgcolor=\"white\">\n";
   print LOGHTML "<center><h1>\n";
   print LOGHTML "Pluglet API Automated TestRun Results\n";
   $date = localtime;
   print LOGHTML "</h1><h2>", $date;
   print LOGHTML "</h2></center>\n";
   print LOGHTML "<hr noshade>";
   print LOGHTML "<table bgcolor=\"#99FFCC\">\n";
   print LOGHTML "<tr bgcolor=\"lightblue\">\n";
   print LOGHTML "<td>Test Case</td>\n";
   print LOGHTML "<td>Result</td>\n";
   print LOGHTML "</tr>\n";
}

#########################################################################
#
# Construct Output HTML file indicating status of each run
#
#########################################################################
sub constructHTMLBody() {
    if (open(LOGTXT,$LOGTXT)) {
	my @all_lines = <LOGTXT>;
	close( LOGTXT ) || print "WARNING: Can't close $LOGTXT,HTML_LOG may be incorrect\n";
	my $line;
	foreach $line ( @all_lines ) {
	    # avoid linebreaks
	    chop $line;  
	    # assuming that all lines are kind'of 'aaa=bbb'
	    ($class, $status) = split /=/, $line;	
	    print LOGHTML "<tr><td>",$class,"</td>";
	    if ($status=~/FAILED/) {
		print LOGHTML "<td><font color=\"red\">",$status,"</font></td></tr>\n";
	    } else {
		print LOGHTML "<td>",$status,"</td></tr>\n";
	    }
	}
    } else {
	print "WARNING: Can't open $LOGTXT,HTML_LOG may be incorrect\n";
    }
}
		
#########################################################################
#
# Construct Output HTML file Footer
#
#########################################################################
sub constructHTMLFooter() {
    print LOGHTML "</table></body></html>\n";
}

#########################################################################
#
# Construct Output HTML file indicating status of each run
#
#########################################################################
sub constructHTML() {
        if (open( LOGHTML, ">$LOGHTML" )) { 
	    constructHTMLHeader();
	    constructHTMLBody();
	    constructHTMLFooter();
	    close( LOGHTML) || print("WARNING: Can't close HTML file... $!\n");
	} else {
	    print("WARNING: Can't open HTML file... $!\n");
	}
}

#########################################################################
#
# Construct LogFile Header. The Log file is always appended with entries
#
#########################################################################
sub constructLogHeader() {
    print LOGFILE "\n";
    print LOGFILE "\n";    
    print LOGFILE $delimiter;
    $date = localtime;
    print LOGFILE "Logging Test Run on $date ...\n";
    print LOGFILE $delimiter;
    print LOGFILE "\n";    	

    print "All Log Entries are maintained in LogFile $LOGFILE\n";
    print "\n";
}

#########################################################################
#
# Construct LogFile Footer. 
#
#########################################################################
sub constructLogFooter() {
    print "\n";
    print LOGFILE $delimiter;
    $date = localtime;
    print LOGFILE "End of Logging Test $date ...\n";
    print LOGFILE $delimiter;
    print "\n"; 
}

########################################################################
#
# Construct Log String
#
########################################################################
sub constructLogString {
    my $logstring = shift(@_);
    print LOGFILE "$logstring\n";

}

########################################################################
#
# Safely append to file : open, append, close.
#
########################################################################
sub safeAppend {
    my $file = shift(@_);
    my $line = shift(@_);
    open (FILE, ">>$file") or die ("Can't open $file");
    print FILE $line;
    close FILE;
}

########################################################################
#
# Loading parameters 
#
########################################################################
sub loadParams {
    my $file = shift(@_);   
    my $key;
    my $value;
    my %params;
    open (PARAMS, "<$file") || die "ERROR: Can't open parameters file $file\n";
    while ($pair = <PARAMS>) {
	chomp($pair);
	$pair=~s#^\s*|\s*$##g;
	($key,$value) = split('=', $pair);
	$params{$key} = $value;
    }
    close(PARAMS) || print "WARNING: Can't close parameters file $file\n";
    return %params;
}
########################################################################
#
# Getting global parameters for suite
#
########################################################################
sub getGlobParam {    
    if (! $glob_params_loaded){
	%glob_params = loadParams("$topdir/config/$glob_params_file_name");
	$glob_params_loaded = 1;
    }
    return $glob_params{shift(@_)};    
}
########################################################################
#
# Getting test parameters 
#
########################################################################
sub getTestParam {
    my $path = getTestDir(shift(@_));
    if (! $test_params_loaded){
	%test_params = loadParams("$path/$test_params_file_name");
	$test_params_loaded = 1;
    }
    return $test_params{shift(@_)};    
}
########################################################################
#
# Getting test directory from testcase 
#
########################################################################
sub getTestDir {    
    my $path = shift(@_);
    $path =~ s/:.*$//;
    return "$topdir/build/test/$path";
}
########################################################################
#
# copy set of test parameters 
#
########################################################################
sub copyTestDataIfAny {
    my $testcase = shift(@_);
    my $num = $testcase;
    if (($num =~ /:/) and ($num =~ s/^.*://g)) {
	my $path = getTestDir($testcase);
	my $curdir = cwd();
	chdir($path);
	my @list = <*.lst>;
	#chdir($curdir); #Commented by avm
	if ($list[0]) {
	    open (LST, $list[0]) || die "ERROR: Can't open file $list[0]\n";
	    while ($tp = <LST>){
		if ($tp =~ /$num: /) {
		    open (TP_OUT, ">$path/$list[0].TEMP") || die "ERROR: Can't open file >$path/$list[0].TEMP";
		    $tp =~ s/$num: //;
		    print TP_OUT $tp;
		    chomp($tp); 
		    print "--Save parameters line \"$tp\" to $path/$list[0].TEMP for test number $num\n";
		    close TP_OUT || print "WARNING: Can't close file $path/$list[0].TEMP\n";
		    chdir($curdir);
		    return "$path/$list[0].TEMP";
		} 
	    }
	}
	chdir($curdir);
    }
    return "";
}
########################################################################
#
# copy set of test parameters 
#
########################################################################
sub checkResultFile {    
    if (open(TEST_RESULT, $test_result_file_name)) {
	$result = <TEST_RESULT>;
	chomp $result;
	$result=~s#^\s*|\s*$##g;
	close(TEST_RESULT) || print "WARNING: Can't close $test_result_file_name";
	if ($result eq "PASSED") {
	    return 1;
	} else {
	    return 0;
	}
    } else {
	print "WARNING: Can't open $test_result_file_name,\n",
	      "test aborted or DELAY_FACTOR so small.\n";
	return 0;
    }
}
########################################################################
#
# running one test case
#
########################################################################
sub runTestCase {
    my $testcase = shift(@_);
    if ($^O=~/Win32/) {
	runTestCaseWin($testcase);
    } else {
	runTestCaseUx($testcase);
    }
}

########################################################################
#
# running one test case under Unix like systems
#
########################################################################
sub runTestCaseUx {
    my $testcase = shift(@_);
    $test_params_loaded = 0;

    
    chdir( $mozhome );
    print "========================================\n";
    $logstr="Running TestCase $testcase....";
    print "$logstr\n";
    constructLogString "$logstr";

    $nom = $testcase;
    $nom =~ s/\/|:/_/g;
    $testlog = "$logdir/$nom.log";

    open( TST, ">$mozhome/StartProperties") || die "ERROR: Can't create $mozhome/StartProperties\n";
    print TST "TEST_TOP_DIR=$topdir\n";
    print TST "TEST_DIR=".getTestDir($testcase)."\n";
    print TST "TEST_CASE=$testcase\n";
    close( TST) || print "WARNING:Can't close $mozhome/StartProperties\n";

    #clear log files ..
    unlink("$mozhome/$test_log_file_name") ;
    unlink("$mozhome/$test_result_file_name");
    $testDataFile = copyTestDataIfAny($testcase);
    $testhtml = getGlobParam("HTML_ROOT")."/".getTestParam($testcase, "TEST_HTML");
    if (!defined($pid = fork())) {
	die "Can't fork: $!";
    } elsif ($pid == 0) { #Child process
	$pluglet = getTestParam($testcase, "PLUGLET");
	if ($pluglet) { #prepare enviroment
	    $saved_classpath = $ENV{"CLASSPATH"};
	    $ENV{"CLASSPATH"} = $ENV{"CLASSPATH"}.$path_separator.$pluglet;
	    $pluglet =~ s/$file_separator[^$file_separator]*$//;
	    $ENV{"PLUGLET"} = $pluglet;
	}
	print "--TestDir is: ".getTestDir($testcase)."\n";
	print "--Use pluglet from: ", $ENV{"PLUGLET"},"\n";
       	print "--Starting ","$mozhome/$program_name $program_args $testhtml\n";
	open( STDOUT, ">$testlog" ) || print "WARNING: Can't redirect STDOUT\n";
	open( STDERR, ">&STDOUT" )  || print "WARNING: Can't redirect STDOUT\n";
	select STDERR; $|=1; select STDOUT; $|=1; 
	exec("$mozhome/$program_name", split(/ /,"$program_args $testhtml")) or print STDERR  "ERROR:cann't start $program_name\n";
	die "Unfortunately exec behaviour .";
    } 
    #this is parent process
    $cnt = 0;
    while (true) {
	sleep($DELAY_OF_CYCLE);
	if (waitpid($pid,&WNOHANG)) { #Test exited
	    $logstr = "Mozilla terminated by signal ".($? & 127)." with exitcode ".($? >> 8);
            constructLogString "$logstr";   
	    if (-e "$mozhome/core") {
	    	print("Test dumped core\n");
		safeAppend $LOGTXT,"$testcase=FAILED\n";
		$logstr="Test dumped core...";
		constructLogString "$logstr";
		$logstr = "Check ErrorLog File $testlog ";
		constructLogString "$logstr";
		constructLogString "";
		unlink "$mozhome/core";
	    }elsif (!checkResultFile()) {
	        print("Register FAILED\n");
		safeAppend $LOGTXT, "$testcase=FAILED\n";
		$logstr = "Test FAILED by autorun.pl ...";
		constructLogString "$logstr";
	    } else {
	    	print("Register PASSED\n");
		safeAppend $LOGTXT, "$testcase=PASSED\n";
	    }
	    last;
	}
	$cnt += $DELAY_OF_CYCLE;
	if ( $cnt >= $DELAY_FACTOR ) {
	    if (checkResultFile) {
		print("Register PASSED\n");
		safeAppend $LOGTXT, "$testcase=PASSED\n";
	    } else {
		print("Register FAILED\n");
		safeAppend $LOGTXT, "$testcase=FAILED\n";
	    }
	    kill  9, $pid;
	    sleep(1); 
	    wait();# take exit code
	    last;
	}
    } # while with sleep
    if (open(TEST_LOG, "$mozhome/$test_log_file_name")) {
	while (<TEST_LOG>) {
	    chomp($_);
	    constructLogString $_;
	}
	close(TEST_LOG);
	unlink("$mozhome/$test_log_file_name") || print "WARNING: Can't remove  $mozhome/$test_log_file_name\n";
    }
    (-f "$mozhome/$test_result_file_name") && unlink("$mozhome/$test_result_file_name");
    (-f $testDataFile) && unlink($testDataFile);
	
}



########################################################################
#
# running one test case under Win32
#
########################################################################

sub runTestCaseWin { 
    my $testcase = shift(@_);
    $test_params_loaded = 0;
    do 'Win32\Process.pm' || die "Can't do Win32\Process.pm $!";
    
    chdir( $mozhome );
    print "========================================\n";
    $logstr="Running TestCase $testcase....";
    print "$logstr\n";
    constructLogString "$logstr";

    #($nom) = ($testcase =~ /([^\.]*)$/);
    $nom = $testcase;
    $nom =~ s/\/|:/_/g;
    $testlog = "$logdir/$nom.log";

    if (open( TST, ">$mozhome/StartProperties")) {
	print TST "TEST_TOP_DIR=$topdir\n";
	print TST "TEST_DIR=".getTestDir($testcase)."\n";
	print TST "TEST_CASE=$testcase\n";
	close( TST) || print "WARNING: Can't close $mozhome/StartProperties\n";
    } else {
	die "ERROR: Can't create $mozhome/StartProperties\n";
    }

#Remove log files from previous execution 
    (-f "$mozhome/$test_log_file_name") && unlink("$mozhome/$test_log_file_name");   
    (-f "$mozhome/$test_result_file_name") && unlink("$mozhome/$test_result_file_name");
    
    $testDataFile = copyTestDataIfAny($testcase);
    $testhtml = getGlobParam("HTML_ROOT")."/".getTestParam($testcase, "TEST_HTML");
    $pluglet = getTestParam($testcase, "PLUGLET");
    if ($pluglet) {
	$saved_classpath = $ENV{"CLASSPATH"};
	$ENV{"CLASSPATH"} = $ENV{"CLASSPATH"}.$path_separator.$pluglet;
	$pluglet =~ s/\\[^\\]*$//;
	$ENV{"PLUGLET"} = $pluglet;
    }
    print "--Use pluglet from: ", $ENV{"PLUGLET"},"\n";
    print "--Starting ","$mozhome/$program_name $program_args $testhtml\n";
    open( SAVEOUT, ">&STDOUT" ) || print "WARNING: Cann't backup STDOUT\n";
    open( SAVEERR, ">&STDERR" ) || print "WARNING: Cann't backup STDERR\n";
    open( STDOUT, ">$testlog" ) || print "WARNING: Cann't redirect STDOUT\n";
    open( STDERR, ">&STDOUT" )  || print "WARNING: Cann't redirect STDOUT\n";
    Win32::Process::Create($ProcessObj,
			   "$mozhome/$program_name",
			   "$mozhome/$program_name $program_args $testhtml",
			   1,
			   NORMAL_PRIORITY_CLASS,
			   "$mozhome" ) || die "ERROR:cann't start $program_name\n";
    close( STDOUT ) || print "WARNING: Cann't close redirected STDOUT\n";
    close( STDERR ) || print "WARNING: Cann't close redirected STDERR\n";
    open( STDOUT, ">&SAVEOUT" ) || print "WARNING: Cann't restore STDOUT\n";
    open( STDERR, ">&SAVEERR" ) || print "WARNING: Cann't restore STDERR\n";
	   	
    #Restore CLASSPATH if it was changed
    if ($pluglet) {
      $ENV{"CLASSPATH"}=$saved_classpath;
    }
    $flag = 0;
    $cnt = 0;
    while (true) {
	sleep($DELAY_OF_CYCLE);
	system("$topdir\\utils\\Killer.exe");
	$ProcessObj->GetExitCode($exit_code);
	if ( $exit_code != $STILL_ALIVE ) {
	    
	    $logstr = "Test terminated with exit code $exit_code.";
	    constructLogString "$logstr";
	    if ( (! checkResultFile) or ($exit_code != 0)) {
		safeAppend $LOGTXT, "$testcase=FAILED\n";
		constructLogString "Test FAILED by autorun.pl ...";
		constructLogString "Check ErrorLog File $testlog ";
		constructLogString "";
	    } else {
		safeAppend $LOGTXT, "$testcase=PASSED\n";
	    }	    
	    $flag = 1;
	    last;
	}
	
	$cnt += $DELAY_OF_CYCLE;
	if ( $cnt >= $DELAY_FACTOR ) {
	    $flag = 0;
	    if (checkResultFile) {
		print("Register PASSED\n");
		safeAppend $LOGTXT, "$testcase=PASSED\n";
	    } else {
		print("Register FAILED\n");
		safeAppend $LOGTXT, "$testcase=FAILED\n";
	    }
	    $ProcessObj->Kill(0);
	    last;
	}
    } # while with sleep

    if (open(TEST_LOG, "$mozhome/$test_log_file_name")) {
	while (<TEST_LOG>) {
	    chomp($_);
	    constructLogString $_;
	}
	close(TEST_LOG);
	unlink("$mozhome/$test_log_file_name");
    }
    (-f "$mozhome/$test_result_file_name") && unlink("$mozhome/$test_result_file_name");
    (-f $testDataFile) && unlink($testDataFile);
	
}
##################################################################
# main
##################################################################
title();

$curdir = cwd();           
$depth="../../../../../..";
$topdir = abs_path($depth);
$date = localtime;
$date =~ s/[ :]/_/g;
$logdir = "$topdir/log/$date";
mkdir($logdir, 0777);

# Prepare file names
$LOGFILE = "$logdir/BWTestRun.log";
$LOGTXT = "$logdir/BWTest.txt";
$LOGHTML = "$logdir/BWTest.html";


$test_params_file_name = "TestProperties";
$glob_params_file_name = "CommonProperties";
$test_log_file_name = "PlugletTest.log";
$test_result_file_name = "PlugletTest.res";

$glob_params_loaded = 0;

# process command-line parameters
# and check for valid usage
$testcase = "";

if ($#ARGV >= 0) {
    if (($#ARGV == 1) and ($ARGV[0] eq "-t")) {
	$testcase = $ARGV[1];
	$runtype = 1;
    } elsif (($#ARGV == 1) and ($ARGV[0] eq "-f")) {
	$LST_IN = "$curdir/$ARGV[1]";
	(-f $LST_IN ) or die "File $LST_IN doesn't exist";
	$runtype =2;
    } else {
	usage;
	die;
    }
} else {
    $runtype = 3;
    $LST_IN = "$curdir/BWTest.lst.ORIG";
    (-f $LST_IN ) or die "File $LST_IN doesn't exist";
}

$mozhome = $ENV{"MOZILLA_FIVE_HOME"};
( $mozhome eq "" ) and die "MOZILLA_FIVE_HOME is not set. Please set it and rerun this script....\n";
( -f "$mozhome/$program_name" ) or die "Could not find $program_name in MOZILLA_FIVE_HOME.\n";

# Prepare TXT log stream
open( LOGFILE, ">>$LOGFILE" ) || die("ERROR: Can't open LOG file... $!\n");
select LOGFILE; $| = 1; select STDOUT;

constructLogHeader();

$DELAY_FACTOR=getGlobParam("DELAY");
$DELAY_OF_CYCLE=getGlobParam("DELAY_OF_CYCLE");

if ($runtype == 1){
    runTestCase($testcase);
} else {
    open( LST_IN, $LST_IN ) || die("ERROR: Can't open $LST_IN...\n");
    while( $testcase = <LST_IN> ) {
	chomp $testcase;
	$testcase=~s#^\s*|\s*$##g;
	if ( ($testcase =~ /^\s*#/)||($testcase eq "")) {
	    next;
	}
	runTestCase($testcase);    
    } 
}    
 
constructLogFooter();
constructHTML();
chdir($curdir);







