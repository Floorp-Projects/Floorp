#!/usr/bin/perl
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is JavaScript Core Tests.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1997-1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#
# Contributers:
#  Robert Ginda
#
# Second cut at runtests.pl script originally by
# Christine Begle (cbegle@netscape.com)
# Branched 11/01/99
#

use Getopt::Mixed "nextOption";

# command line option defaults
local $opt_classpath = "";
local $opt_engine_type = "";
local $opt_output_file = "";
local @opt_test_list_files;
local @opt_neg_list_files;
local $opt_suite_path = "./";
local $opt_shell_path = "";
local $opt_java_path = "";
local $opt_bug_url = "http://bugzilla.mozilla.org/show_bug.cgi?id=";
local $opt_trace = 0;
local $opt_console_failures = 0;
local $opt_lxr_url = "http://lxr.mozilla.org/mozilla/source/js/tests/";

# command line option definition
local $options = "b=s bugurl>b c=s classpath>c e=s engine>e f=s file>f " .
  "h help>h i j=s javapath>j k confail>k l=s list>l L=s neglist>L " .
  "p=s testpath>p s=s shellpath>s t trace>t u=s lxrurl>u";

&parse_args;

local $os_type = &get_os_type;
local $engine_command = &get_engine_command;
local @test_list = &get_test_list;

local $user_exit = 0;
local $html = "";
local @failed_tests;
local $failures_reported = 0;
local $tests_completed = 0;
local $exec_time_string;
local $start_time = time;

if ($os_type ne "WIN") {
    # on unix, ^C pauses the tests, and gives the user a chance to quit, but 
    # report on what has been done, to just quit, or to continue (the
    # interrupted test will still be skipped.)
    # windows doesn't handle the int handler they way we want it to,
    # so don't even pretend to let the user continue.
    $SIG{INT} = 'int_handler';
}

if ($#test_list == -1) {
    die ("Nothing to test.\n");
}

&execute_tests (@test_list);

local $exec_time = (time - $start_time);
local $exec_hours = int($exec_time / 60 / 60);
local $exec_mins = int($exec_time / 60);
local $exec_secs = ($exec_time % 60);

if ($exec_hours > 0) {
    $exec_time_string = "$exec_hours hours, $exec_mins minutes, " .
      "$exec_secs seconds";
} elsif ($exec_mins > 0) {
    $exec_time_string = "$exec_mins minutes, $exec_secs seconds";
} else {
    $exec_time_string = "$exec_secs seconds";
}

&write_results;

#End.

sub execute_tests {
    local (@test_list) = @_;
    local $test, $shell_command, $line, @output;
    local $file_param = " -f ";
    local $last_suite, $last_test_dir;
    local $failure_lines;

    &status ("Executing " . ($#test_list + 1) . " test(s).");

    foreach $test (@test_list) {
        local ($suite, $test_dir, $test_file) = split("/", $test);
        # *-n.js is a negative test, expect exit code 3 (runtime error)
        local $expected_exit = ($test =~ /\-n\.js$/) ? 3 : 0;
        local $got_exit, $exit_signal;
        local $bug_line;

        # user selected [Q]uit from ^C handler.
        if ($user_exit) {
            return;
        }
        
        # Append the shell.js files to the shell_command if they're there.
        # (only check for their existance if the suite or test_dir has changed
        # since the last time we looked.)
        if ($last_suite ne $suite || $last_test_dir ne $test_dir) {
            $shell_command = $engine_command;
            
            if (-f $opt_suite_path . $suite . "/shell.js") {
                $shell_command .= $file_param . $opt_suite_path . $suite .
                  "/shell.js";
            }
            if (-f $opt_suite_path . $suite . "/" . $test_dir . "/shell.js") {
                $shell_command .= $file_param . $opt_suite_path . $suite .
                  "/" . $test_dir . "/shell.js";
            }
            
            $last_suite = $suite;
            $last_test_dir = $test_dir;
        }
        
        &dd ("executing: " . $shell_command . $file_param . $opt_suite_path .
             $test);
        
        open (OUTPUT, $shell_command . $file_param . $opt_suite_path . $test .
              " 2>&1 |");	
        @output = <OUTPUT>;
        close (OUTPUT);
        
        if ($os_type ne "WIN") {
            # unix systems have signal information in the lower 8 bits
            $got_exit = ($? >> 8);
            $exit_signal = ($? & 255);
        } else {
            # windows doesn't have signal information at all, $? is the exit code
            # period.
            $got_exit = $?;
            $exit_signal = 0;
        }

        $failure_lines = "";
        $bug_line = "";

        foreach $line (@output) {

            # watch for testcase to proclaim what exit code it expects to
            # produce (0 by default)
            if ($line =~ /expect(ed)?\s*exit\s*code\s*\:?\s*(\n+)/i) {
                $expected_exit = $1;
                &dd ("Test case expects exit code $expect_exit");
            }

            # watch for failures
            if ($line =~ /failed!/i) {
                $failure_lines .= $line;
            }

            # and watch for bugnumbers
            # XXX This only allows 1 bugnumber per testfile, should be
            # XXX modified to allow for multiple.
            if ($line =~ /bugnumber\s*\:?\s*(.*)/i) {
                $1 =~ /(\n+)/;
                $bug_line = "<a href='$opt_bug_url$1' target='other_window'>" .
                  "Bug Number $1</a>";
            }

        }
        
        if (!@output) {
            @output = ("Testcase produced no output!");
        }

        if ($got_exit != $expected_exit) {
            # full testcase output dumped on mismatched exit codes,
            &report_failure ($test, "Expected exit code " .
                             "$expected_exit, got $got_exit\n" .
                             "Testcase terminated with signal $exit_signal\n" .
                             "Complete testcase output was:\n" .
                             join ("\n",@output), $bug_line);
        } elsif ($failure_lines) {
            # only offending lines if exit codes matched
            &report_failure ($test, "Failure messages were:\n" . $failure_lines,
                             $bug_line);
        }        
        
        &dd ("exit code $got_exit, exit signal $exit_signal.");

        $tests_completed++;
    }
}

sub write_results {
    local $list_name, $neglist_name;
    local $completion_date = localtime;
    local $failure_pct = int(($failures_reported / $tests_completed) * 10000) /
      100;
    &dd ("Writing output to $opt_output_file.");

    if ($#opt_test_list_files == -1) {
        $list_name = "All tests";
    } elsif ($#opt_test_list_files < 10) {
        $list_name = join (", ", @opt_test_list_files);
    } else {
        $list_name = "($#opt_test_list_files test files specified)";
    }

    if ($#opt_neg_list_files == -1) {
        $neglist_name = "(none)";
    } elsif ($#opt_test_list_files < 10) {
        $neglist_name = join (", ", @opt_neg_list_files);
    } else {
        $neglist_name = "($#opt_neg_list_files skip files specified)";
    }
    
    open (OUTPUT, "> $opt_output_file") ||
      die ("Could not create output file $opt_output_file");

    print OUTPUT 
      ("<html><head>\n" .
       "<title>Test results, $opt_engine_type</title>\n" .
       "</head>\n" .
       "<body bgcolor='white'>\n" .
       "<a name='tippy_top'></a>\n" .
       "<h2>Test results, $opt_engine_type</h2><br>\n" .
       "<p class='results_summary'>\n" .
       "Test List: $list_name<br>\n" .
       "Skip List: $neglist_name<br>\n" .
       ($#test_list + 1) . " test(s) selected, $tests_completed test(s) " .
       "completed, $failures_reported failures reported " .
       "($failure_pct% failed)<br>\n" .
       "Engine command line: $engine_command<br>\n" .
       "OS type: $os_type<br>\n");

    if ($opt_engine_type eq "rhino") {
        open (JAVAOUTPUT, $opt_path_to_java . "java -fullversion 2>&1 |");
        print OUTPUT <JAVAOUTPUT>;
        print OUTPUT "<BR>";
        close (JAVAOUTPUT);
    }
     
    print OUTPUT 
      ("Testcase execution time: $exec_time_string.<br>\n" .
       "Tests completed on $completion_date.<br><br>\n");

    if ($failures_reported > 0) {
        print OUTPUT
          ("[ <a href='#fail_detail'>Failure Details</a> | " .
           "<a href='#retest_list'>Retest List</a> | " .
           "<a href='menu.html'>Test Selection Page</a> ]<br>\n" .
           "<hr>\n" .
           "<a name='fail_detail'></a>\n" .
           "<h2>Failure Details</h2><br>\n<dl>" .
           $html .
           "</dl>\n[ <a href='#tippy_top'>Top of Page</a> | " .
           "<a href='#fail_detail'>Top of Failures</a> ]<br>\n" .
           "<hr>\n<pre>\n" .
           "<a name='retest_list'></a>\n" .
           "<h2>Retest List</h2><br>\n" .
           "# Retest List, $opt_engine_type, " .
           "generated $completion_date.\n" .
           "# Original test base was: $list_name.\n" .
           "# $tests_completed of " . ($#test_list + 1) .
           " test(s) were completed, " .
           "$failures_reported failures reported.\n" .
           join ("\n", @failed_tests) .
           "</pre>\n" .
           "[ <a href='#tippy_top'>Top of Page</a> | " .
           "<a href='#retest_list'>Top of Retest List</a> ]<br>\n");
    } else {
        print OUTPUT 
          ("<h1>Whoop-de-doo, nothing failed!</h1>\n");
    }
    
    print OUTPUT "</body>";

    close (OUTPUT);

    &status ("Wrote results to '$opt_output_file'.");

    if ($opt_console_failures) {
        &status ("$failures_reported test(s) failed");
    }

}
 
sub parse_args {
    local $option, $value;
    
    &dd ("checking command line options.");
    
    Getopt::Mixed::init ($options);
    $Getopt::Mixed::order = $Getopt::Mixed::RETURN_IN_ORDER;
    
    while (($option, $value) = nextOption()) {

        if ($option eq "b") {
            &dd ("opt: setting bugurl to '$value'.");
            $opt_bug_url = $value;
            
        } elsif ($option eq "c") {
            &dd ("opt: setting classpath to '$value'.");
            $opt_classpath = $value;
            
        } elsif ($option eq "e") {            
            &dd ("opt: setting engine to $value.");
            $opt_engine_type = $value;
            
        } elsif ($option eq "f") {
            if (!$value) {
                die ("Output file cannot be null.\n");
            }
            &dd ("opt: setting output file to '$value'.");
            $opt_output_file = $value;
            
        } elsif ($option eq "h") {
            &usage;

        } elsif ($option eq "j") {
            if (!($value =~ /[\/\\]$/)) {
                $value .= "/";
            }
            &dd ("opt: setting java path to '$value'.");
            $opt_java_url = $value;
            
        } elsif ($option eq "k") {
            &dd ("opt: displaying failures on console.");
            $opt_console_failures=1;

        } elsif ($option eq "l" || (($option eq "") && ($lastopt eq "l"))) {
            $option = "l";
            &dd ("opt: adding test list '$value'.");
            push (@opt_test_list_files, $value);

        } elsif ($option eq "L" || (($option eq "") && ($lastopt eq "L"))) {
            $option = "L";
            &dd ("opt: adding negative list '$value'.");
            push (@opt_neg_list_files, $value);
            
        } elsif ($option eq "p") {
            $opt_suite_path = $value;
            if (!($opt_suite_path =~ /[\/\\]$/)) {
                $opt_suite_path .= "/";
            }
            &dd ("opt: setting suite path to '$opt_suite_path'.");
            
        } elsif ($option eq "s") {
            $opt_shell_path = $value;
            if (!($opt_shell_path =~ /[\/\\]$/)) {
                $opt_shell_path .= "/";
            }
            &dd ("opt: setting shell path to '$opt_shell_path'.");
            
        } elsif ($option eq "t") {
            &dd ("opt: tracing output.  (console failures at no extra charge.)");
            $opt_console_failures = 1;
            $opt_trace = 1;
            
        } elsif ($option eq "u") {
            &dd ("opt: setting lxr url to '$value'.");
            $opt_lxr_url = $value;
            
        } else {
            &usage;
        }

        $lastopt = $option;

    }
    
    Getopt::Mixed::cleanup();
    
    if (!$opt_engine_type) {
        die "You must select a shell to test in.\n";
    }

    if (!$opt_output_file) {
        $opt_output_file = "results-" . $opt_engine_type . "-" . 
          &get_tempfile_id . ".html";
    }
    
    &dd ("output file is '$opt_output_file'");

}

#
# print the arguments that this script expects
#
sub usage {
    print STDERR 
      ("\nusage: $0 [<options>] \n" .
       "(-b|--bugurl)             Bugzilla URL.\n" .
       "                          (default is $opt_bug_url)\n" .
       "(-c|--classpath)          Classpath (Rhino only.)\n" .
       "(-e|--engine) <type>      Specify the type of engine to test. <type> " .
       "is one of \n" .
       "                          (smopt|smdebug|lcopt|lcdebug|xpcshell|" .
       "rhino).\n" .
       "(-f|--file) <file>        Redirect output to file named <file>.\n" .
       "                          (default is " .
       "results-<engine-type>-<date-stamp>.html)\n" .
       "(-h|--help)               Print this message.\n" .
       "(-j|--javapath)           Location of java executable.\n" .
       "(-k|--confail)            Log failures to console (also.)\n" . 
       "(-l|--list) <file> ...    List of tests to execute.\n" . 
       "(-L|--neglist) <file> ... List of tests to skip.\n" . 
       "(-p|--testpath) <path>    Root of the test suite. (default is ./)\n" .
       "(-s|--shellpath) <path>   Location of JavaScript shell.\n" .
       "(-t|--trace)              Trace script execution.\n" .
       "(-u|--lxrurl) <url>       Complete URL to tests subdirectory on lxr.\n" .
       "                          (default is $opt_lxr_url)\n\n");    
    exit (1);
    
}

#
# get the shell command used to start the (either) engine
#
sub get_engine_command {
    
    local $retval;
    
    if ($opt_engine_type eq "rhino") {
        &dd ("getting rhino engine command.");
        $retval = &get_rhino_engine_command;
    } elsif ($opt_engine_type eq "xpcshell") {
        &dd ("getting xpcshell engine command.");
        $retval = &get_xpc_engine_command;	
    } elsif ($opt_engine_type =~ /^lc(opt|debug)$/) {
        &dd ("getting liveconnect engine command.");
        $retval = &get_lc_engine_command;	
    } elsif ($opt_engine_type =~ /^sm(opt|debug)$/) {
        &dd ("getting spidermonkey engine command.");
        $retval = &get_sm_engine_command;	
    } else {
        die ("Unknown engine type selected, '$opt_engine_type'.\n");
    }
    
    &dd ("got '$retval'");
    
    return $retval;
    
}

#
# get the shell command used to run rhino
#
sub get_rhino_engine_command {    
    local $retval = $opt_java_path . "java ";
    
    if ($opt_shell_path) {
        $opt_classpath = ($opt_classpath) ?
          $opt_classpath . ":" . $opt_shell_path :
            $opt_shell_path;
    }
    
    if ($opt_classpath) {
        $retval .= "-classpath $opt_classpath ";
    }
    
    $retval .= "org.mozilla.javascript.tools.shell.Main";
    
    return $retval;
    
}

#
# get the shell command used to run xpcshell
#
sub get_xpc_engine_command {    
    local $m5_home = @ENV{"MOZILLA_FIVE_HOME"} ||
      die ("You must set MOZILLA_FIVE_HOME to use the xpcshell" ,
           ($os_type eq "WIN") ? "." : ", also " .
           "setting LD_LIBRARY_PATH to the same directory may get rid of " .
           "any 'library not found' errors.\n");

    if (($os_type ne "WIN") && (!@ENV{"LD_LIBRARY_PATH"})) {
        print STDERR "-#- WARNING: LD_LIBRARY_PATH is not set, xpcshell may " .
          "not be able to find the required components.\n";
    }
    
    if (!($m5_home =~ /[\/\\]$/)) {
        $m5_home .= "/";
    }

    return $m5_home . "xpcshell";

}

#
# get the shell command used to run spidermonkey
#
sub get_sm_engine_command {
    local $retval;
    
    if ($os_type eq "WIN") {
        # spidermonkey on windows
        
        if ($opt_shell_path) {
            $retval = $opt_shell_path;
            if (!($retval =~ /[\/\\]$/)) {
                $retval .= "/";
            }
        } else {
            if ($opt_engine_type eq "smopt") {
                $retval = "../src/Release/";
            } else {
                $retval = "../src/Debug/";
            }
        }
        $retval .= "jsshell.exe";
        
    } else {
        # spidermonkey on un*x
        
        if ($opt_shell_path) {
            $retval = $opt_shell_path;
            if (!($retval =~ /[\/\\]$/)) {
                $retval .= "/";
            }
        } else {
            $retval = $opt_suite_path . "../src/";
            opendir (SRC_DIR_FILES, $retval);
            local @src_dir_files = readdir(SRC_DIR_FILES);
            closedir (SRC_DIR_FILES);
            
            local $dir, $object_dir;
            local $pattern = ($opt_engine_type eq "smdebug") ?
              'DBG.OBJ' : 'OPT.OBJ';

            # scan for the first directory matching
            # the pattern expected to hold this type (debug or opt) of engine
            foreach $dir (@src_dir_files) {
                if ($dir =~ $pattern) {
                    $object_dir = $dir;
                    break;
                }
            }
            
            if (!$object_dir) {
                die ("Could not locate an object directory in $retval " .
                     "matching the pattern *$pattern.  Have you built the " .
                     "engine?\n");
            }
            
            $retval .= $object_dir . "/";
        }
        
        $retval .= "js";
        
    } 
    
    if (!(-x $retval)) {
        die ("$retval is not a valid executable on this system.\n");
    }
    
    return $retval;
    
}

#
# get the shell command used to run the liveconnect shell
#
sub get_lc_engine_command {
    local $retval;    
        
    if ($opt_shell_path) {
        $retval = $opt_shell_path;
        if (!($retval =~ /[\/\\]$/)) {
            $retval .= "/";
        }
    } else {
        $retval = $opt_suite_path . "../src/liveconnect/";
        opendir (SRC_DIR_FILES, $retval);
        local @src_dir_files = readdir(SRC_DIR_FILES);
        closedir (SRC_DIR_FILES);
        
        local $dir, $object_dir;
        local $pattern = ($opt_engine_type eq "lcdebug") ?
          'DBG.OBJ' : 'OPT.OBJ';
        
        foreach $dir (@src_dir_files) {
            if ($dir =~ $pattern) {
                $object_dir = $dir;
                break;
            }
        }
        
        if (!$object_dir) {
            die ("Could not locate an object directory in $retval " .
                 "matching the pattern *$pattern.  Have you built the " .
                 "engine?\n");
            }
        
        $retval .= $object_dir . "/";
    }
    
    if ($os_type eq "WIN") {
        $retval .= "lcshell.exe";
    } else {
        $retval .= "lcshell";
    }

    if (!(-x $retval)) {
        die ("$retval is not a valid executable on this system.\n");
    }
    
    return $retval;
    
}

sub get_os_type {    
    local $uname = `uname -a`;
    
    if ($uname =~ /WIN/) {
        $uname = "WIN";
    } else {
        chop $uname;
    }
    
    &dd ("get_os_type returning '$uname'.");
    return $uname;
    
}

sub get_test_list {
    local @test_list;
    local @neg_list;

    if ($#opt_test_list_files > -1) {
        local $list_file;

        &dd ("getting test list from user specified source.");

        foreach $list_file (@opt_test_list_files) {
            push (@test_list, &expand_user_test_list($list_file));
        }
    } else {
        &dd ("no list file, groveling in '$opt_suite_path'.");
        @test_list = &get_default_test_list($opt_suite_path);
    }

    if ($#opt_neg_list_files > -1) {
        local $list_file;
        local $orig_size = $#test_list + 1;
        local $actually_skipped;

        &dd ("getting negative list from user specified source.");

        foreach $list_file (@opt_neg_list_files) {
            push (@neg_list, &expand_user_test_list($list_file));
        }

        @test_list = &subtract_arrays (\@test_list, \@neg_list);

        $actually_skipped = $orig_size - ($#test_list + 1);

        &dd ($actually_skipped . " of " . ($#test_list + 1) .
             " tests will be skipped.");
        &dd ((($#neg_list + 1) - $actually_skipped) . " skip tests were " .
             "not actually part of the test list.");


    }
    
    return @test_list;
    
}

#
# reads $list_file, storing non-comment lines into an array.
# lines in the form suite_dir/[*] or suite_dir/test_dir/[*] are expanded
# to include all test files under the specified directory
#
sub expand_user_test_list {
    local ($list_file) = @_;
    local @retval = ();

    if ($list_file =~ /\.js$/ || -d $list_file) {

        push (@retval, &expand_test_list_entry($list_file));

    } else {

        open (TESTLIST, $list_file) || 
          die("Error opening test list file '$list_file': $!\n");
    
        while (<TESTLIST>) {
            s/\n$//;
            if (!(/\s*\#/)) {
                # It's not a comment, so process it
                push (@retval, &expand_test_list_entry($_));
            }
        }

        close (TESTLIST);

    }
    
    return @retval;
    
}

sub expand_test_list_entry {
    local ($entry) = @_;
    local @retval;

    if ($entry =~ /\.js$/) {
        # it's a regular entry, add it to the list
        if (-f $entry) {
            push (@retval, $entry);
        } else {
            status ("testcase '$entry' not found.");
        }
    } elsif ($entry =~ /(.*\/[^\*][^\/]*)\/?\*?$/) {
        # Entry is in the form suite_dir/test_dir[/*]
        # so iterate all tests under it
        local $suite_and_test_dir = $1;
        local @test_files = &get_js_files ($opt_suite_path . 
                                           $suite_and_test_dir);
        local $i;
        
        foreach $i (0 .. $#test_files) {
            $test_files[$i] = $suite_and_test_dir . "/" . 
              $test_files[$i];
        }
        
        splice (@retval, $#retval + 1, 0, @test_files);
        
    } elsif ($entry =~ /([^\*][^\/]*)\/?\*?$/) {
        # Entry is in the form suite_dir[/*]
        # so iterate all test dirs and tests under it
        local $suite = $1;
        local @test_dirs = &get_subdirs ($opt_suite_path . $suite);
        local $test_dir;
        
        foreach $test_dir (@test_dirs) {
            local @test_files = &get_js_files ($opt_suite_path . $suite . "/" .
                                               $test_dir);
            local $i;
            
            foreach $i (0 .. $#test_files) {
                $test_files[$i] = $suite . "/" . $test_dir . "/" . 
                  $test_files[$i];
            }
            
            splice (@retval, $#retval + 1, 0, @test_files);
        }
        
    } else {
        die ("Dont know what to do with list entry '$entry'.\n");
    }

    return @retval;

}

#
# Grovels through $suite_path, searching for *all* test files.  Used when the
# user doesn't supply a test list.
#
sub get_default_test_list {
    local ($suite_path) = @_;
    local @suite_list = &get_subdirs($suite_path);
    local $suite;
    local @retval;

    foreach $suite (@suite_list) {
        local @test_dir_list = get_subdirs ($suite_path . $suite);
        local $test_dir;
        
        foreach $test_dir (@test_dir_list) {
            local @test_list = get_js_files ($suite_path . $suite . "/" .
                                             $test_dir);
            local $test;
            
            foreach $test (@test_list) {
                $retval[$#retval + 1] = $suite_path . $suite . "/" . $test_dir .
                  "/" . $test;
            }
        }
    }
    
    return @retval;
    
}

#
# generate an output file name based on the date
#
sub get_tempfile_id {
    local ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) =
      &get_padded_time (localtime);
    
    return $year . "-" . $mon . "-" . $mday . "-" . $hour . $min . $sec;
    
}

sub get_padded_time {
    local ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = @_;
    
    $mon++;
    $mon = &zero_pad($mon);
    $year= ($year < 2000) ? "19" . $year : $year;
    $mday= &zero_pad($mday);
    $sec = &zero_pad($sec);
    $min = &zero_pad($min);
    $hour = &zero_pad($hour);
    
    return ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst);
    
}

sub zero_pad {
    local ($string) = @_;
    
    $string = ($string < 10) ? "0" . $string : $string;
    return $string;
}

sub subtract_arrays {
    local ($whole_ref, $part_ref) = @_;
    local @whole = @$whole_ref;
    local @part = @$part_ref;
    local $line;

    foreach $line (@part) {
        @whole = grep (!/$line/, @whole); 
    }

    return @whole;

}

#
# given a directory, return an array of all subdirectories
#
sub get_subdirs {
    local ($dir)  = @_;
    local @subdirs;
    
    if (!($dir =~ /\/$/)) {
        $dir = $dir . "/";
    }
    
    opendir (DIR, $dir) || die ("couldn't open directory $dir: $!");
    local @testdir_contents = readdir(DIR);
    closedir(DIR);
    
    foreach (@testdir_contents) {
        if ((-d ($dir . $_)) && ($_ ne 'CVS') && ($_ ne '.') && ($_ ne '..')) {
            @subdirs[$#subdirs + 1] = $_;
        }
    }

    return @subdirs;
}

#
# given a directory, return an array of all the js files that are in it.
#
sub get_js_files {
    local ($test_subdir) = @_;
    local @js_file_array;
    
    opendir (TEST_SUBDIR, $test_subdir) || die ("couldn't open directory " .
                                                "$test_subdir: $!");
    @subdir_files = readdir(TEST_SUBDIR);
    closedir( TEST_SUBDIR );
    
    foreach (@subdir_files) {
        if ($_ =~ /\.js$/) {
            $js_file_array[$#js_file_array+1] = $_;
        }
    }
    
    return @js_file_array;
}

sub report_failure {
    local ($test, $message, $bug_line) = @_;

    $failures_reported++;

    if ($opt_console_failures) {
        print STDERR ("*-* Testcase $test failed:\n$message\n");
    }

    $message =~ s/\n/<br>\n/g;
    $html .= "<a name='failure$failures_reported'></a>";
    if ($opt_lxr_url) {
        $test =~ /\/?([^\/]+\/[^\/]+\/[^\/]+)$/;
        $test = $1;
        $html .= "<dd><b>".
          "Testcase <a target='other_window' href='$opt_lxr_url$test'>$1</a> " .
            "failed</b> $bug_line<br>\n";
    } else {
        $html .= "<dd><b>".
          "Testcase $test failed</b> $bug_line<br>\n";
    }
    
    $html .= " [ ";
    if ($failures_reported > 1) {
        $html .= "<a href='#failure" . ($failures_reported - 1) . "'>" .
          "Previous Failure</a> | ";
    }

    $html .= "<a href='#failure" . ($failures_reported + 1) . "'>" .
      "Next Failure</a> | " .
        "<a href='#tippy_top'>Top of Page</a> ]<br>\n" .
          "<tt>$message</tt><br>\n";

    @failed_tests[$#failed_tests + 1] = $test;

}

sub dd {
    
    if ($opt_trace) {
        print ("-*- ", @_ , "\n");
    }
    
}

sub status {
    
    print ("-#- ", @_ , "\n");
    
}

sub int_handler {
    local $resp;

    do {
        print ("\n** User Break: Just [Q]uit, Quit and [R]eport, [C]ontinue ?");
        $resp = <STDIN>;
    } until ($resp =~ /[QqRrCc]/);

    if ($resp =~ /[Qq]/) {
        print ("User Exit.  No results were generated.\n");
        exit;
    } elsif ($resp =~ /[Rr]/) {
        $user_exit = 1;
    }

}
