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
    print "\n";
    print "##################################################################\n";
    print "\n";
   
}

##################################################################
# Title display
##################################################################
sub title() {
	
    print "\n";
    print "################################################\n";
    print "   Automated Execution of Java Pluglet API TestSuite\n";
    print "################################################\n";
    print "\n";
    print "\n";
    print "\n";
    print "\n";
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
# Append table entries to Output HTML File 
#
#########################################################################
sub appendEntries() {
   print LOGHTML "<tr><td></td><td></td></tr>\n";
   print LOGHTML "<tr><td></td><td></td></tr>\n";
   print LOGHTML "<tr bgcolor=\"#FF6666\"><td>Test Status (XML)</td><td>Result</td></tr>\n";

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
   print LOGHTML "<tr bgcolor=\"#FF6666\">\n";
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
	
    open( MYLOG, $LOGTXT ) || print "WARNING: Cann't open $LOGTXT,HTML_LOG may be incorrect\n";
    @all_lines = <MYLOG>;
    close( MYLOG ) || print "WARNING: Cann't close $LOGTXT,HTML_LOG may be incorrect\n";
    
    my $line;
    foreach $line ( sort @all_lines ) {
        # avoid linebreaks
	chop $line;  
	# assuming that all lines are kind'of 'aaa=bbb'
	($class, $status) = split /=/, $line;	
	
	print LOGHTML "<tr><td>",$class,"</td><td>",$status,"</td></tr>\n";
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
	constructHTMLHeader;
	constructHTMLBody;
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
    open (FILE, ">>$file") or die ("Cann't open $file");
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
    my $key, $value, %params;
    open (PARAMS, "<$file") || print "WARNING: Can't open parameters file $file\n";
    while ($pair = <PARAMS>) {
	chomp($pair);
	$pair=~s#^\s*|\s*$##g;
	($key,$value) = split('=', $pair);
	$params{$key} = $value;
	print "$key,$value\n";
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
    my $path;
    $path = getTestDir(shift(@_));
    print "TestDir is $path\n";
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
	    open (LST, $list[0]) || die "Cann't open file $list[0]\n";
	    while ($tp = <LST>){
		if ($tp =~ /$num: /) {
		    open (TP_OUT, ">$path/$list[0].TEMP") || die "Cann't open file >$path/$list[0].TEMP";
		    $tp =~ s/$num: //;
		    print TP_OUT $tp;
		    chomp($tp); 
		    print "--Save parameters line \"$tp\" to $path/$list[0].TEMP for test number $num\n";
		    close TP_OUT || die "Cann't close file $path/$list[0].TEMP\n";
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
    open(TEST_RESULT, $test_result_file_name) || print "Cann't open $test_result_file_name,\n",
                                                        "test aborted or DELAY_FACTOR so small.\n";
    $result = <TEST_RESULT>;
    chomp $result;
    $result=~s#^\s*|\s*$##g;
    close(TEST_RESULT);
    if ($result eq "PASSED") {
	return 1;
    } else {
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

    open(LST_OUT, ">$LST_OUT") or die ("Cann't open LST_OUT file... $!\n");
    print LST_OUT $testcase;
    close(LST_OUT) || die ("Cann't close LST_OUT file...\n");

    chdir( $mozhome );
    print "========================================\n";
    $logstr="Running TestCase $testcase....";
    print "$logstr\n";
    constructLogString "$logstr";

    $nom = $testcase;
    $nom =~ s/\/|:/_/g;
    $testlog = "$logdir/$nom.log";

    open( TST, ">$mozhome/StartProperties");
    print TST "TEST_TOP_DIR=$topdir\n";
    print TST "TEST_DIR=".getTestDir($testcase)."\n";
    print TST "TEST_CASE=$testcase\n";
    close( TST);

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
	print "--Use pluglet from: ", $ENV{"PLUGLET"},"\n";
       	print "--Starting ","$mozhome/$program_name $program_args $testhtml\n";
	exec("$mozhome/$program_name $program_args $testhtml 2>&1 1>$testlog") or print STDERR  "ERROR:cann't start $program_name\n";
	die "Unfortunately exec behaviour .";
    } 
    #parent ..
    $flag = 0;
    $cnt = 0;
    $grppid = getpgrp(0);
    print "Enter to parent ....";
    while (true) {
	print "Sleep to $DELAY_OF_CYCLE -- $pid\n";
	sleep($DELAY_OF_CYCLE);
	$ret = waitpid($pid,&WNOHANG);
	if ($ret) { #Test exited
	    print "Test exited ...";
	    if (-e "$mozhome/core") {
		safeAppend $LOGTXT,"$testcase=FAILED\n";
		$logstr="Test dumped core...";
		constructLogString "$logstr";	    
	    }
	    if (!checkResultFile()) {
		safeAppend $LOGTXT, "$testcase=FAILED\n";
		$logstr = "Test FAILED by autorun.pl ...";
		constructLogString "$logstr";
	    } else {
		safeAppend $LOGTXT, "$testcase=PASSED\n";
	    }
	    $logstr = "Check ErrorLog File $testlog ";
	    constructLogString "$logstr";
	    constructLogString "";
	    constructLogString "";
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
	    print "kill $pid by timeout\n";
	    @processes=`ps -o pgid,pid,cmd|grep $grppid|grep $program_name`;
	    @pids;
	    for($i=0;$i<=$#processes;$i++) {
		$processes[$i]=~/\s*$grppid\s+(.*?\s)/;
		$pids[$i]=$1;
	    }
	    kill  9, @pids;
	    last;
	}
    } # while with sleep
    print "\nWHILE exited\n";
    open(TEST_LOG, "$mozhome/$test_log_file_name") ;
    #|| print "WARNING: Cann't open $mozhome/$test_log_file_name \n";
    while (<TEST_LOG>) {
	chomp($_);
	constructLogString $_;
    }
    close(TEST_LOG) ;
    #|| print "WARNING: Cann't close $mozhome/$test_log_file_name \n";
    unlink("$mozhome/$test_log_file_name") || 
	print "WARNING: Cann't remove  $mozhome/$test_log_file_name\n";
    unlink("$mozhome/$test_result_file_name") || 
	print "WARNING: Cann't remove  $mozhome/$test_result_file_name\n";
    unlink($LST_OUT) || 
	print "WARNING: Cann't remove  $LST_OUT\n";	
    $testDataFile and unlink($testDataFile);
	
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
    open(LST_OUT, ">$LST_OUT") or die ("Cann't open LST_OUT file...\n");
    print LST_OUT $testcase;
    close(LST_OUT) || die ("Cann't close LST_OUT file...\n");

    chdir( $mozhome );
    print "========================================\n";
    $logstr="Running TestCase $testcase....";
    print "$logstr\n";
    constructLogString "$logstr";

    #($nom) = ($testcase =~ /([^\.]*)$/);
    $nom = $testcase;
    $nom =~ s/\/|:/_/g;
    $testlog = "$logdir/$nom.log";

    open( TST, ">$mozhome/StartProperties");
    print TST "TEST_TOP_DIR=$topdir\n";
    print TST "TEST_DIR=".getTestDir($testcase)."\n";
    print TST "TEST_CASE=$testcase\n";
    close( TST);

    unlink("$mozhome/$test_log_file_name") ;
    #|| print "WARNING: Cann't remove$mozhome/$test_log_file_name\n";
    unlink("$mozhome/$test_result_file_name") ;
    #|| print "WARNING: Cann't remove $mozhome/$test_result_file_name\n";
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
	
	$ProcessObj->GetExitCode($exit_code);
	if ( $exit_code != $STILL_ALIVE ) {
	    
	    $logstr = "Test terminated with exit code $exit_code.";
	    constructLogString "$logstr";
	    if ( (! checkResultFile) or ($exit_code != 0)) {
		safeAppend $LOGTXT, "$testcase=FAILED\n";
		$logstr = "Test FAILED by autorun.pl ...";
		constructLogString "$logstr";
	    } else {
		safeAppend $LOGTXT, "$testcase=PASSED\n";
	    }
	    $logstr = "Check ErrorLog File $testlog ";
	    constructLogString "$logstr";
	    constructLogString "";
	    constructLogString "";
	    
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

    open(TEST_LOG, "$mozhome/$test_log_file_name") ;
    #|| print "WARNING: Cann't open $mozhome/$test_log_file_name \n";
    while (<TEST_LOG>) {
	chomp($_);
	constructLogString $_;
    }
    close(TEST_LOG) ;
    #|| print "WARNING: Cann't close $mozhome/$test_log_file_name \n";
    unlink("$mozhome/$test_log_file_name") || 
	print "WARNING: Cann't remove  $mozhome/$test_log_file_name\n";
    unlink("$mozhome/$test_result_file_name") || 
	print "WARNING: Cann't remove  $mozhome/$test_result_file_name\n";
    unlink($LST_OUT) || 
	print "WARNING: Cann't remove  $LST_OUT\n";	
    $testDataFile and unlink($testDataFile);
	
}
##################################################################
# main
##################################################################
title;

$curdir = cwd();           

$topdir = "$curdir/../../../../../..";
$date = localtime;
$date =~ s/[ :]/_/g;
$logdir = "$topdir/log/$date";
mkdir($logdir, 0777);

# Prepare file names
$LOGFILE = "$logdir/BWTestRun.log";
$LOGTXT = "$logdir/BWTest.txt";
$LOGHTML = "$logdir/BWTest.html";

$LST_OUT = "$logdir/BWTest.lst.out";
$test_params_file_name = "TestProperties";
$glob_params_file_name = "CommonProperties";
$test_log_file_name = "PlugletTest.log";
$test_result_file_name = "PlugletTest.res";

$HTML_ROOT = "HTML_ROOT";
$TEST_HTML = "TEST_HTML";

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

# Here must come a piece of code, that determinates 
# apprunner instance, removes core, but there's no
# core under win32

$id=$$;

# Prepare log streams
open( LOGFILE, ">>$LOGFILE" ) or die("Can't open LOG file... $!\n");
select LOGFILE; $| = 1; select STDOUT;
open( LOGHTML, ">$LOGHTML" ) or die("Can't open HTML file... $!\n");

#$ENV{"CLASSPATH"} = "$topdir/build/classes".$path_separator.$ENV{"CLASSPATH"};

constructLogHeader();
constructHTMLHeader();

$DELAY_FACTOR=getGlobParam("DELAY");
$DELAY_OF_CYCLE=getGlobParam("DELAY_OF_CYCLE");

if ($runtype == 1){
    runTestCase($testcase);
} else {
    open( LST_IN, $LST_IN ) or die("Can't open $LST_IN...\n");
    while( $line = <LST_IN> ) {
	chomp $line;
	$line=~s#^\s*|\s*$##g;
	$testcase = $line;
	if ( $testcase =~ /^\s*#/ ) {
	    next;
	}
	runTestCase($testcase);    
    } 
}    
 
constructLogFooter;
constructHTMLBody;
constructHTMLFooter;

$newfile="$mozhome/BWProperties.$id";
rename "$newfile", "$mozhome/BWProperties";
chdir($curdir);
