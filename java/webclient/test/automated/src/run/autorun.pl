#!<PERL_DIR>/perl
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
# This script is used to invoke automated test cases for Webclient API
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
#Initialization procedure
#
# STILL_ALIVE is a constant defined in winbase.h and indicates what
# process still alive
###################################################################
sub init() {
    #assume, that script is placed to build/run directory           
    $depth="../..";
    $topdir = abs_path($depth);
    $curdir = cwd();
    $start_date = localtime();
    $date=$start_date;
    $date =~ s/[ :]/_/g;

    $logdir = "$topdir/log/$date";
    mkdir($logdir, 0777);

    $COMMON_LOG_FILE = "$logdir/BWTestRun.log";
    $COMMON_RESULT_FILE = "$logdir/BWTest.txt";
    $COMMON_RESULT_HTML_FILE = "$logdir/BWTest.html";

    $TEST_LOG_FILE = "$curdir/WebclientTest.log";
    $TEST_RESULT_FILE = "$curdir/WebclientTest.res";
    $TEST_INIT_FILE = "$curdir/StartProperties";

    $COMMON_PARAMS_FILE = "$topdir/config/CommonProperties";
   
    $test_params_file_name = "TestProperties";
    $runner_class = "org.mozilla.webclient.test.basic.TestRunner";    
    %params=();
    $delimiter="###################################################\n";
    if ($^O=~/Win32/) {
        $STILL_ALIVE = 0x103;
        $path_separator = ";";
        $file_separator = "\\";
        $JAVA="java.exe";
    } else  { #Unix systems
        $path_separator = ":";
        $file_separator = "\/"; 
        $JAVA="java";
    }
    $javahome=$ENV{"JAVAHOME"};
    unless(-f "$javahome$file_separator$JAVA") {
        die "Can't find java executable $javahome$file_separator$JAVA";
    }
}
##################################################################
# Usage
##################################################################
sub usage() {

    print "\n";
    print "##################################################################\n";
    print "   perl autorun.pl [ -m ] [ -f <test cases list file> ]\n";
    print "\n";
    print " where <test cases list file> is file with test cases list.\n";
    print "\n";
    print "or:\n";
    print "   perl autorun.pl [ -m ] [ -t <test case> ]\n";
    print "\n";
    print " where <test case> is one of the test cases IDs\n";
    print " for ex: basic/api/History_getURLForIndex:2\n";
    print "\n";
    print "The [ -m ] switch is used to run \"mixed\" tests.\n";
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
    print "        Automated Execution of Java Webclient TestSuite\n";
    print "##################################################################\n";
    print "\n";
    print "\n";
}

#########################################################################
#
# Construct LogFile Header. The Log file is always appended with entries
#
#########################################################################
sub constructLogHeader() {
    print COMMON_LOG_FILE "\n";
    print COMMON_LOG_FILE "\n";    
    print COMMON_LOG_FILE $delimiter;
    print COMMON_LOG_FILE "Logging Test Run on $start_date ...\n";
    print COMMON_LOG_FILE $delimiter;
    print COMMON_LOG_FILE "\n";         

    print "All Log Entries are maintained in LogFile $COMMON_LOG_FILE\n";
    print "\n";
}

#########################################################################
#
# Construct LogFile Footer. 
#
#########################################################################
sub constructLogFooter() {
    print "\n";
    print COMMON_LOG_FILE $delimiter;
    $date = localtime;
    print COMMON_LOG_FILE "End of Logging Test $date ...\n";
    print COMMON_LOG_FILE $delimiter;
    print "\n"; 
}

########################################################################
#
# Construct Log String
#
########################################################################
sub constructLogString {
    my $logstring = shift(@_);
    print COMMON_LOG_FILE "$logstring\n";

}

#########################################################################
#
# Construct Output HTML file indicating status of each run
#
#########################################################################

sub constructHTML() {
    unless(open(LOGHTML,">$COMMON_RESULT_HTML_FILE")) {
        print("WARNING: Can't open output HTML file\n");
        return;
    }
    print LOGHTML "<html><head><title>\n";
    print LOGHTML "Webclient API Test Status\n";
    print LOGHTML "</title></head><body bgcolor=\"white\">\n";
    print LOGHTML "<center><h1>\n";
    print LOGHTML "Webclient API Automated TestRun Results</h1></center>\n";
    print LOGHTML "<center><h2>",getParam("BUILD_DESCRIPTION"),"</h2></center>\n";
    print LOGHTML "<center><h3>Started: ", $start_date,"</h3></center>\n";
    $date=localtime();
    print LOGHTML "<center><h3>Ended: ", $date,"</h3></center>\n";
    print LOGHTML "<hr noshade>";
    print LOGHTML "<table bgcolor=\"#99FFCC\">\n";
    print LOGHTML "<tr bgcolor=\"lightblue\">\n";
    print LOGHTML "<td>Test Case</td>\n";
    print LOGHTML "<td>Result</td>\n";
    print LOGHTML "<td>Comment</td>\n";
    print LOGHTML "</tr>\n";
    if (open(LOGTXT,$COMMON_RESULT_FILE)) {
        my @all_lines = <LOGTXT>;
        close( LOGTXT ) || print "WARNING: Can't close $LOGTXT,HTML_LOG may be incorrect\n";
        my $line;
        foreach $line ( @all_lines ) {
            chop $line;
            $line=~/^(.*?=)(.*?=)(.*)$/;
            $class = $1;
            $status = $2;
            $comment = $3;
            $class =~ s/=$//g;
            $status =~ s/=$//g;
	    $detailed_log="$class.log";
	    $detailed_log=~ s/\/|:/_/g;
            print LOGHTML "<tr><td><a href=",$detailed_log,">",$class,"</a></td>";
            if ($status=~/FAILED/) {
                print LOGHTML "<td><font color=\"red\">",$status,"</font></td>\n";
            } elsif ($status=~/UNIMPLEMENTED/) {
                print LOGHTML "<td><font color=\"yellow\">",$status,"</font></td>\n";
            } else {
                print LOGHTML "<td>",$status,"</td>\n";
            }
            print  LOGHTML "<td>",$comment,"</td></tr>\n"
        }
    } else {
        print "WARNING: Can't open $LOGTXT,HTML_LOG may be incorrect\n";
    }
    print LOGHTML "</table></body></html>\n";
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
# Loading all parameters for current testcase 
#
########################################################################
sub loadAllParams {
    my $testcase = shift(@_);
    my %params=();
    %params = loadParams($COMMON_PARAMS_FILE,\%params);
    %params = loadParams(getTestDir($testcase).$file_separator.$test_params_file_name,\%params);
    return %params;
}
########################################################################
#
# Loading parameters 
#
########################################################################
sub loadParams {
    my $file = shift(@_); 
    my $params = shift(@_); 
    my $key;
    my $value;
    open (PARAMS, "<$file") || die "ERROR: Can't open parameters file $file\n";
    while ($pair = <PARAMS>) {
        chomp($pair);
        $pair=~s#^\s*|\s*$##g;
        ($key,$value) = split('=', $pair);
        $$params{$key} = $value;
    }
    close(PARAMS) || print "WARNING: Can't close parameters file $file\n";
    return %$params;
}
########################################################################
#
# Getting parameter
#
########################################################################
sub getParam {        
    return $params{shift(@_)};    
}

########################################################################
#
# Getting test directory from testcase 
#
########################################################################
sub getTestDir {    
    my $path = shift(@_);
    print "$path\n";
    $path =~ s/:.*$//;
    print "$path\n";
    return "$topdir/build/test/$path";
}
########################################################################
#
# copy set of test parameters 
#
########################################################################
sub getCurrentTestArguments {
    my $testcase = shift(@_);
    my $num = $testcase;
    my $tp="";
    if (($num =~ /:/) and ($num =~ s/^.*:|\s*$//g)) {
        my $path = getTestDir($testcase);
        print "+++++$path+++\n";
        my $list = "$path/Test.lst";
        open (LST, $list) || die "ERROR: Can't open file $list\n";
        while ($tp = <LST>){
            print "--$tp--$num--\n";
            unless($tp =~ /^\s*\#/) { #Avoid comments
                if ($tp =~ /$num:/) {
                    $tp =~ s/$num://;
                    $tp =~ s/^\s*|\s*$//g;
                    print "--Current args are: \"$tp\"\n";
                    close (LST);
                    return $tp;
                }
            } 
        }
        close (LST);
    }
    return "";
}
########################################################################
#
# Verify results of test execution 
#
########################################################################
sub checkResultFile {
    my $logstring = "";
    my $resstring = "";
    my $result = "";
    if (open(TEST_RESULT, $TEST_RESULT_FILE)) {
        $result = <TEST_RESULT>;
        chomp $result;
        $result=~s#^\s*|\s*$##g;
        close(TEST_RESULT) || constructLogString "WARNING: Can't close $TEST_RESULT_FILE";
        if ($result=~/^PASSED/){ 
            $resstring="$testcase=$result";
            $logstring="PASSED due to execution result - $result";
        } elsif ($result=~/^FAILED/){ 
            $resstring="$testcase=$result";
            $logstring="FAILED due to execution result - $result";
        } elsif($result=~/^UNIMPLEMENTED/) {
            $resstring="$testcase=$result";
            $logstring="UNIMPLEMENTED due to execution result - $result";
        }else {
            $resstring="$testcase=FAILED=By autorun.pl. Unknown content of result file - $result";
            $logstring="FAILED due to strnge content of result file - $result";
        }
    } else {
        $resstring = "$testcase=FAILED=By autorun.pl: Can't open file with result.Test aborted or execution time so small";
        $logstring = "FAILED by autorun.pl: Can't open file with result.Test aborted or execution time so small";
    }
    if (-e "$curdir/core") {
	unless($ignorecore) {
	    $resstring = "$testcase=FAILED=By autorun.pl. Test dumped core. The result of execution was: $resstring";
	    $logstring = "FAILED by autorun.pl. Test DUMPED CORE. Original log was: $logstring";
	}else {
	    $resstring.= " Warning: Coredump was ignored.";
	    $logstring.= " Warning: Coredump was ignored."
	}
    }
    safeAppend $COMMON_RESULT_FILE, "$resstring\n";
    constructLogString "$logstring";
}
########################################################################
#
# running one test case
#
########################################################################
sub runTestCase {
    my $testcase = shift(@_);
    %params=loadAllParams($testcase);
    $DELAY_EXTERNAL=getParam("DELAY_EXTERNAL");
    $DELAY_OF_CYCLE=getParam("DELAY_OF_CYCLE");
    if ($^O=~/Win32/) {
        runTestCaseWin($testcase);
    } else {
        print "Unix systems isn't well tested now. Should be done\n";
        runTestCaseUx($testcase);
    }
}
########################################################################
#
# running one test case under Unix
#
########################################################################
sub runTestCaseUx { 
    my $testcase = shift(@_);
    
    print "========================================\n";
    $logstr="Running TestCase $testcase....";
    print "$logstr\n";
    constructLogString "$logstr";

    $nom = $testcase;
    $nom =~ s/\/|:/_/g;
    $testlog = "$logdir/$nom.log";

    if (open( TST, ">".$TEST_INIT_FILE)) {
        print TST "TEST_TOP_DIR=$topdir\n";
        print TST "BROWSER_BIN_DIR=".getParam("BROWSER_BIN_DIR")."\n";
        print TST "TEST_ID=$testcase\n";
        print TST "TEST_ARGUMENTS=".getCurrentTestArguments($testcase);
        close( TST) || print "WARNING: Can't close $TEST_INIT_FILE\n";
    } else {
        die "ERROR: Can't create $TEST_INIT_FILE $!\n";
    }

#Remove log files from previous execution 
    (-f "$TEST_LOG_FILE") && unlink("$TEST_LOG_FILE");   
    (-f "$TEST_RESULT_FILE") && unlink("$TEST_RESULT_FILE");
    (-f "$curdir/core") && unlink("$curdir/core");
    if (!defined($pid = fork())) {
        die "Can't fork: $!";
    } elsif ($pid == 0) { #Child process
        $args="-native -Djava.library.path=".getParam("BROWSER_BIN_DIR").":".$ENV{"LD_LIBRARY_PATH"};
        $ENV{"PATH"}=$ENV{"PATH"}.";".getParam("BROWSER_BIN_DIR");
        print "--Starting $javahome/$JAVA $args $runner_class $modifier $TEST_INIT_FILE\n";
        open( SAVEOUT, ">&STDOUT" ) || print "WARNING: Cann't backup STDOUT\n";
        open( SAVEERR, ">&STDERR" ) || print "WARNING: Cann't backup STDERR\n";
        open( STDOUT, ">$testlog" ) || print "WARNING: Cann't redirect STDOUT\n";
        open( STDERR, ">&STDOUT" )  || print "WARNING: Cann't redirect STDOUT\n";
        select STDERR; $|=1; select STDOUT; $|=1; 
        exec("$javahome/$JAVA $args $runner_class $modifier $TEST_INIT_FILE") or print STDERR  "ERROR:cann't start $javahome/$JAVA\n";
        die "Unfortunately exec behaviour .";
    } 
    #Parent process  
    $cnt = 0;
    while (true) {
        sleep($DELAY_OF_CYCLE);
        if (waitpid($pid,&WNOHANG)) { #If process aleady died ..
            checkResultFile();
            last;
        } else { #If process is still alive     
            $cnt += $DELAY_OF_CYCLE;
            if ( $cnt >= $DELAY_EXTERNAL) { 
                constructLogString "WARNING: Process doesn't succesfuly died. DELAY_OF_CYCLE should be increased";
                checkResultFile();
                kill  9, $pid;
                sleep(1); 
                wait();# take exit code
                last;
            }
        }
    } # while with sleep

    if (open(TEST_LOG, "$TEST_LOG_FILE")) {
        while (<TEST_LOG>) {
            chomp($_);
            constructLogString $_;
        }
        close(TEST_LOG);
        unlink("$TEST_LOG_FILE");
    }
    (-f "$TEST_RESULT_FILE") && unlink("$TEST_RESULT_FILE");
    (-f "$TEST_INIT_FILE") && unlink("$TEST_INIT_FILE");
        
}
########################################################################
#
# running one test case under Win32
#
########################################################################

sub runTestCaseWin { 
    my $testcase = shift(@_);
    do 'Win32\Process.pm' || die "Can't do Win32\Process.pm $!";
    
    print "========================================\n";
    $logstr="Running TestCase $testcase....";
    print "$logstr\n";
    constructLogString "$logstr";

    $nom = $testcase;
    $nom =~ s/\/|:/_/g;
    $testlog = "$logdir/$nom.log";

    if (open( TST, ">".$TEST_INIT_FILE)) {
        print TST "TEST_TOP_DIR=$topdir\n";
        print TST "BROWSER_BIN_DIR=".getParam("BROWSER_BIN_DIR")."\n";
        print TST "TEST_ID=$testcase\n";
        print TST "TEST_ARGUMENTS=".getCurrentTestArguments($testcase);
        close( TST) || print "WARNING: Can't close $TEST_INIT_FILE\n";
    } else {
        die "ERROR: Can't create $TEST_INIT_FILE $!\n";
    }

#Remove log files from previous execution 
    (-f "$TEST_LOG_FILE") && unlink("$TEST_LOG_FILE");   
    (-f "$TEST_RESULT_FILE") && unlink("$TEST_RESULT_FILE");
    
    $args="-Djava.library.path=".getParam("BROWSER_BIN_DIR");
    $ENV{"PATH"}=$ENV{"PATH"}.";".getParam("BROWSER_BIN_DIR");
    print "--Starting $javahome/$JAVA $args $runner_class $modifier $TEST_INIT_FILE\n";
    open( SAVEOUT, ">&STDOUT" ) || print "WARNING: Cann't backup STDOUT\n";
    open( SAVEERR, ">&STDERR" ) || print "WARNING: Cann't backup STDERR\n";
    open( STDOUT, ">$testlog" ) || print "WARNING: Cann't redirect STDOUT\n";
    open( STDERR, ">&STDOUT" )  || print "WARNING: Cann't redirect STDOUT\n";
    Win32::Process::Create($ProcessObj,
                           "$javahome/$JAVA",
                           "$javahome/$JAVA $args $runner_class  $modifier $TEST_INIT_FILE",
                           1,
                           NORMAL_PRIORITY_CLASS,
                           "$curdir" ) || die "ERROR:cann't start $JAVA\n";
    close( STDOUT ) || print "WARNING: Cann't close redirected STDOUT\n";
    close( STDERR ) || print "WARNING: Cann't close redirected STDERR\n";
    open( STDOUT, ">&SAVEOUT" ) || print "WARNING: Cann't restore STDOUT\n";
    open( STDERR, ">&SAVEERR" ) || print "WARNING: Cann't restore STDERR\n";
  
    $cnt = 0;
    while (true) {
        sleep($DELAY_OF_CYCLE);
        #print("Running $topdir\\utils\\Killer.exe $testlog\n");
        system("$topdir\\utils\\Killer.exe $testlog");
        $ProcessObj->GetExitCode($exit_code);
        if ( $exit_code != $STILL_ALIVE ) {  #If process aleady died ..
            constructLogString "Test terminated with exit code $exit_code.";
            checkResultFile() ;
            #if ( (! $temp_result) or ($exit_code != 0)) {
            last;
        } else { #If process is still alive     
            $cnt += $DELAY_OF_CYCLE;
            if ( $cnt >= $DELAY_EXTERNAL) { 
                checkResultFile();
                $ProcessObj->Kill(0);
                last;
            }
        }
    } # while with sleep

    if (open(TEST_LOG, "$TEST_LOG_FILE")) {
        while (<TEST_LOG>) {
            chomp($_);
            print("Construct log string: $_");
            constructLogString $_;
        }
        close(TEST_LOG);
        unlink("$TEST_LOG_FILE");
    }
    (-f "$TEST_RESULT_FILE") && unlink("$TEST_RESULT_FILE");
    (-f "$TEST_INIT_FILE") && unlink("$TEST_INIT_FILE");
        
}
##################################################################
# main
##################################################################

title();
init();
# process command-line parameters
# and check for valid usage

$i=0;
$modifier = "";
$testcase = undef;
$ignorecore = undef;
$LST_IN = undef;
$runtype = -1;
while($i<=$#ARGV) {
    if ($ARGV[$i] eq "-i") {
	$ignorecore=1;
	$i++;
	next;
    }elsif ($ARGV[$i] eq "-m") {
	$modifier="-m";
	if($runtype==-1) {
	    $runtype = 2;
	    $LST_IN = "BWMixedTest.lst";
	}
	$i++;
	next;
    }elsif ($ARGV[$i] eq "-t") {
	if(defined $LST_IN) {
	    usage();
	    exit -1;
	}
	if(defined $ARGV[$i+1]) {
	    $testcase = $ARGV[$i+1];
	    $runtype = 1;
	    $i+=2;
	    next;
	} else {
	    usage;
	    exit -1;
	}
    }elsif ($ARGV[$i] eq "-f") {
	if(defined $testcase) {
	    usage();
	    exit -1;
	}
	if(defined $ARGV[$i+1]) {
	    $LST_IN = $ARGV[$i+1];
	    $runtype = 2;
	    $i+=2;
	    next;
	} else {
	    usage;
	    exit -1;
	}
    }else {
	usage();
	exit -1;
    }
}
if($runtype==-1) {
    $modifier = "";
    $testcase = "";
    $LST_IN = "BWTest.lst";
    $runtype = 2;
}

# Prepare TXT log stream
open( COMMON_LOG_FILE, ">$COMMON_LOG_FILE" ) || die("ERROR: Can't open $COMMON_LOG_FILE file... $!\n");
select COMMON_LOG_FILE; $| = 1; select STDOUT;

constructLogHeader();

if ($runtype == 1){
    runTestCase($testcase);
} else {
    (-f $LST_IN ) or die "ERROR: File $LST_IN doesn't exist";
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








