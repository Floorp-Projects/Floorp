#!/usr/bin/perl
#
# ***** BEGIN LICENSE BLOCK ***** 
# Version: MPL 1.1/GPL 2.0/LGPL 2.1 
#
# The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
# "License"); you may not use this file except in compliance with the License. You may obtain 
# a copy of the License at http://www.mozilla.org/MPL/ 
#
# Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
# WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
# language governing rights and limitations under the License. 
#
# modified by dschaffe@adobe.com for use with tamarin tests
# original file created by christine@netscape.com
#
# Alternatively, the contents of this file may be used under the terms of either the GNU 
# General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
# License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
# LGPL are applicable instead of those above. If you wish to allow use of your version of this 
# file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
# version of this file under the terms of the MPL, indicate your decision by deleting provisions 
# above and replace them with the notice and other provisions required by the GPL or the 
# LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
# under the terms of any one of the MPL, the GPL or the LGPL. 
# 
# ***** END LICENSE BLOCK ***** */
#
# simple script that executes tamarin certification tests.  you have to build the
# stand-alone, avmplus executable.  
# see http://developer.mozilla.org/en/docs/Tamarin_Build_Documentation 
#
# this is just a quick-n-dirty script.  for full reporting, you need to run
# the test driver, which requires java and is currently not available on
# mozilla.org.
#
# this test looks for an executable avmplus shell in
# %MOZ_SRC/mozilla/js/tamarin/platform/,
#

use Cwd 'abs_path';
use Cwd 'getcwd';

# define global variables
@test_search_list=();
@test_list=();
$allpasses=0;
$allfails=0;
@failmsgs=();
$asc="";
$globalabc="";

&parse_args;
&setup_env;
&find_tests;
&execute_tests;
&cleanup_env;

#
# given an array of directories and/or files find all .as test files below the directory including
# all subdirectories.  ignore CVS directory and shell.as files.
#
sub find_tests {
    local @subdir_files;
    local $dir;
    local $file;
    while ($#test_search_list > -1) {
        $dir=pop(@test_search_list);
        if ( -e $dir && $dir =~ /\.as$/ && ! ($dir eq "shell.as") ) {
            $test_list[$#test_list+1]=$dir;
        } elsif ( -d $dir ) {
            opendir ( TEST_SUBDIR, $dir );
            @subdir_files = readdir( TEST_SUBDIR );
            closedir( TEST_SUBDIR );
            foreach ( @subdir_files ) {
                $file=$_;
                if ( $file =~ /\.as$/ && ! ($file =~ /Util.as$/) && ! ($file eq "shell.as") ) {
                    $test_list[$#test_list+1]=$dir . "/" . $file;
                }
               if ( -d ($dir . "/" . $file)
                  && (! ( -e ($dir . "/" . $file . ".as")))
                  && ! ($file eq "CVS")
                  && ! ($file eq ".")
                  && ! ($file eq "..")
               ){
                   $test_search_list[$#test_search_list+1]= $dir . "/" . $file;
               }
           }
        }
    }
}

#
# given an array of test.as files, execute the vm against the .abc file.
# if $js_quiet is set only write failed testcases.  An html report is output
# and information is printed to stdout.
#
sub execute_tests {
    &js_print ("Executing " . ($#test_list+1) . " tests against vm:" .
        " $shell_command\n\n", "<p><tt>", "</tt></p>" );

    while ($#test_list>-1) {
        $js_test=pop(@test_list);

        # counter for passes/fails in the current test
        local $lpass=0;
        local $lfail=0;

        # convert .as name to .abc
        $js_test_abc = substr($js_test, 0, rindex($js_test,".")) .  ".abc";

        # create the test command
        $test_command =
            '"' . $shell_command . '" ' .
            '"' . $js_test_abc . '"';

        if ( !$js_quiet ) {
            &js_print( ($#test_list+1) . " running $js_test\n","<b>","</b><br/>");
        } else {
            print '.';
        }

        if ( !-e $js_test_abc ) {
           if (! -e $asc) {
              die("ERROR! cannot build $js_test, ASC environment variable or -asc must be set to asc.jar.\n");
           }   
           if (! -e $globalabc) {
              die("ERROR! global.abc does not exist, GLOBALABC environment variable or -globalabc must be set to global.abc.\n");
           }
           &compile_test($js_test);
        }

        if ( !-e $js_test_abc ) {
            &js_print( "FAILED! file not found $js_test_abc\n",
                "<font color=#990000>", "</font><br>\n");
            $lfail++;
            $failmsgs[$#failmsgs+1]="$js_test_abc : FAILED! file not found\n";
        } else {
            if (($js_verbose || $js_debug) && !$js_quiet) {
               print $test_command . "\n";
            }
            open( RUNNING_TEST,  "$test_command" . ' 2>&1 |');
        }

	# this is where we want the tests to provide a lot more information
	# that this script must parse so that we can  

       	while( <RUNNING_TEST> ){
            if ($js_debug && !$js_quiet) {
                print $_ . "\n";
            }
	          if ( $_ =~ /PASSED!/ ) {
		            $lpass++;
	          }
            if ( $_ =~ /FAILED!/ ) {
                $lfail++;
                &js_print( "   $_\n","<font color=#990000>","</font><br/>");
                $failmsgs[$#failmsgs+1]="$js_test_abc : $_";
            }
    	  }
    	  close( RUNNING_TEST );
        #
        # figure out whether the test passed or failed.  print out an
        # appropriate level of output based on the value of $as_quiet
        #
        $allfails+=$lfail;
        $allpasses+=$lpass;
        if ( $lpass == 0 && $lfail == 0 ) {
            $lfail=1;
            $allfails++;
            &js_print("   FAILED contained no testcase messages\n","<font color=#990000>","</font><br/>");
            $failmsgs[$#failmsgs+1]="$js_test_abc : FAILED contained no testcase messages\n";
        }
        $result = $lfail>0 ? "FAILED" : "PASSED";
        if (!$js_quiet) {
            &js_print("   $result passes:$lpass fails:$lfail\n","","<br/>");
        }
   }
}
sub compile_test {
    my $js_test=@_[0];
    my $cmd;
    if ( $asc =~ /jar$/ ) {
        $cmd = "java -jar " . $asc;
    } else {
        $cmd = $asc;
    }
    $cmd = $cmd . " -import " . $globalabc;
    my $dir=substr($js_test,0,rindex($js_test,'/'));
    my $file=substr($js_test,rindex($js_test,'/')+1);
    if (! $js_quiet) {
        &js_print( "   compiling " . $file );
    }
    while ( true ) {
        if ( -e $dir . "/shell.as") {
            $cmd=$cmd . " -in " . $dir . "/shell.as";
            last;
        }
        if (index($dir,"/")==-1) {
            last;
        }    
        $dir=substr($dir,0,rindex($dir,"/"));
    }
    $dir=substr($js_test,0,rindex($js_test,'.'));
    $dir=$dir;
    if ( -d $dir ) {
        opendir ( TESTDIR, $dir );
        @dirfiles=readdir (TESTDIR);
        closedir (TESTDIR);
        for (@dirfiles) {
            if ( $_ =~ /\.as$/ ) {
                $cmd = $cmd . " -in " . $dir . "/" . $_;
            }
        }
    }
    $dir=substr($js_test,0,rindex($js_test,'/'));
    opendir (TESTDIR, $dir);
    @dirfiles=readdir (TESTDIR);
    closedir (TESTDIR);
    for (@dirfiles) {
        if ( $_ =~ /Util\.as$/ ) {
            $cmd = $cmd . " -in " . $dir . "/" . "/" . $_;
        }
    }    
    $cmd=$cmd . " " . $js_test;

    if ($js_debug && !$js_quiet) {
        print "\n" . $cmd . "\n";
    }
    open( COMPILE_TEST,  "$cmd" . ' 2>&1 |');
    while ( <COMPILE_TEST> ) {
        if (($js_verbose || $js_debug) && !$js_quiet) {
           print $_;
        }
    }
    close ( COMPILE_TEST);
    $js_test_abc=substr($js_test,0,rindex($js_test,'.')) . ".abc";
    if ( -e $js_test_abc ) {
        print " -> ok\n";
    } else {
        print " -> failed!\n";
    }
}

#
# figure out what os we're on, the default name of the object directory
#
sub setup_env {
    if ( ! -e $shell_command) {
      $shell_command = $ENV{"AVM"}
        || die( "You need to set your AVM environment variable to the avm excutable.\n" );
    }

    if (! -e $asc) {
        $asc = &fix_path($ENV{"ASC"});
    }
    
    if (! -e $globalabc) {
        $globalabc = &fix_path($ENV{"GLOBALABC"});
    }

    $shell_command = &fix_path($shell_command);
    if ( ! -e $shell_command ) {
        die ("Could not find avm executable $shell_command.\n" .
            "Check the value of your AVM environment variable.\n");
    }

    # set the output file name.  let's base its name on the date and platform,
    # and give it a sequence number.

    if ( $get_output ) {
        $js_output = &fix_path(&get_output);
    }
    if ($js_output) {
        print( "Writing results to $js_output\n" );
        open( AS_OUTPUT, "> ${js_output}" ) ||
            die "Can't open log file $js_output\n";
        close AS_OUTPUT;
    }

    # get the start time
    $start_time = time;

    # print out some nice stuff
    $start_date = &get_date;
    &js_print( "Tamarin tests started: " . $start_date, "<p><tt>", "</tt></p>" );
}

#
# parse arguments.  see usage for what arguments are expected.
#
sub parse_args {
    $i = 0;
    while( $i < @ARGV ){
        if ( $ARGV[$i] eq '--v' ) {
            $js_verbose = 1;
        } elsif ( $ARGV[$i] eq '--d' ) {
            $js_debug = 1;
        } elsif ($ARGV[$i] eq '--q' ) {
            $js_quiet = 1;
        } elsif ($ARGV[$i] eq '--h' ) {
            die &usage;
        } elsif ( $ARGV[$i] eq '-E' ) {
            $shell_command = $ARGV[$i+1];
            $shell_command = &fix_path($shell_command);
            $i++;
        } elsif ( $ARGV[$i] eq "-asc" ) {
            $asc = &fix_path($ARGV[$i+1]);
            $i++;
        } elsif ( $ARGV[$i] eq "-globalabc" ) {
            $globalabc = &fix_path($ARGV[$i+1]);
            $i++;
        } elsif ( $ARGV[$i] eq '-f' ) {
            $js_output = &fix_path($ARGV[++$i]);
        } elsif ( -e $ARGV[$i] || -d $ARGV[$i] ) {
            $test_search_list[$#test_search_list+1]=fix_path($ARGV[$i]);
        } else {
            print "unknown argument @ARGV[$i]\n";
            die &usage;
        }
        $i++;
    }
    if ($#test_search_list==-1) {
        $test_search_list[0]=fix_path(getcwd());
    }

    #
    # if no output options are provided, show some output and write to file
    #
    if ( !$js_verbose && !$js_output && !$get_output ) {
        $get_output = 1;
    }
}

#
# print the arguments that this script expects
#
sub usage {
    die ("usage: $0\n" .
        "--q       Quiet mode -- only show information for tests that failed\n".
        "--d       Debug mode -- show all information\n" .
        "--v       Verbose mode  -- show all test status\n" .
        "--o       Send output to file whose generated name is based on date\n".
        "--h       Prints usage\n" .
        "-E <file> Set the avm executable (overwrites AVM env variable)\n" .
        "-f <file> Redirect output to file named <file>\n" .
        "-asc <file> Set the path to asc.jar file\n" .
        "-globalabc <file> Set the path to global.abc file.\n" .
        "[file|dir ..] Add files or directories to test search path\n"
        );
}

#
# if $js_output is set, print to file as well as stdout
#
sub js_print {
    ($string, $start_tag, $end_tag) = @_;

    if ($js_output) {
        open( AS_OUTPUT, ">> ${js_output}" ) ||
            die "Can't open log file $js_output\n";

        print AS_OUTPUT "$start_tag $string $end_tag";
        close AS_OUTPUT;
    }
    print $string;
}

#
# close open files
#
sub cleanup_env {
    # print out some nice stuff
    $end_date = &get_date;
    &js_print( "\nTests complete at $end_date", "<hr><tt>", "</tt>" );

    # print out how long it took to complete
    $end_time = time;

    $test_seconds = ( $end_time - $start_time );

    &js_print( "Start Date: $start_date", "<tt><br>" );
    &js_print( "End Date  : $end_date", "<br>" );
    if ($test_seconds > 59) {
       $test_mins=sprintf("%d",$test_seconds/60);
       $test_seconds=$test_seconds%60;
       &js_print( "Test Time : $test_mins minutes $test_seconds seconds\n\n", "<br>" );
    } else {
       &js_print( "Test Time : $test_seconds seconds\n\n", "<br>" );
    }
    &js_print( "passes    : $allpasses\n","<br>");
    &js_print( "failures  : $allfails\n\n","<br>");

    if ($js_output ) {
        if ( !$js_verbose) {
            print "Results were written to " . $js_output ."\n";
        }
        close AS_OUTPUT;
    }
    if ($#failmsgs > -1) {
        &js_print("\nFAILURES:\n","","<br/>");
        for (@failmsgs) {
            &js_print("  $_","","<br/>");
        }
    }
}

#
# get the current date and time
#
sub get_date {
    &get_localtime;
    $now = $year ."/". $mon ."/". $mday ." ". $hour .":".
        $min .":". $sec ."\n";
    return $now;

}
sub get_localtime {
    ( $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst ) =
        localtime;
    $mon++;
    $mon = &zero_pad($mon);
    if ( $year < 1970 && $year > 100 ) {
       if ($year < 110) {
         $year = "200" . ($year-100);
       } else {
         $year = "20" . ($year-100);
       }
    } else {
       if ($year < 10) {
         $year = "190" . $year;
       } else {
         $year = "19" . $year;
       }
    }
    $mday= &zero_pad($mday);
    $sec = &zero_pad($sec);
    $min = &zero_pad($min);
    $hour = &zero_pad($hour);
}
sub zero_pad {
    local ($string) = @_;
    $string = ($string < 10) ? "0" . $string : $string;
    return $string;
}

#
# generate an output file name based on the date
#
sub get_output {
    &get_localtime;

    $js_output = getcwd() . "/" . $year .'-'. $mon .'-'. $mday ."\.1.html";

    $output_file_found = 0;

    while ( !$output_file_found ) {
        if ( -e $js_output ) {
        # get the last sequence number - everything after the dot
            @seq_no = split( /\./, $js_output, 2 );
            $js_output = $seq_no[0] .".". (++$seq_no[1]) . "\.html";
        } else {
            $output_file_found = 1;
        }
    }
    return $js_output;
}
sub fix_path {
    my $file = @_[0];
    if (-e $file || -d $file) {
        $file=abs_path($file);
    }
    if ($^O eq "cygwin" || $^O =~ /Win32/ ) {
        if ( $file =~ /^\/cygdrive/ ) {
            $file=substr($file,9);
        }
        if ( $file =~ /^\/[A-Za-z]\// ) {
            $file=substr($file,1,1) . ":" . substr($file,2);
        }
    }
    return $file;
}
