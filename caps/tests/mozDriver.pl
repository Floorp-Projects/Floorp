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
# The Original Code is jsDriver.js by Robert Ginda.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1997-2000 Netscape Communications Corporation. All
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
#  Robert Ginda <rginda@netscape.com>
#  John Taylor <jtaylor@netscape.com>
#
# Requires Getopt::Mixed.pm file. If not on system, available from
# http://www.cpan.org
#
# For information about this file, see:
# http://www.mozilla.org/projects/security/components/driver/
#
# Config:
# ------------------
# You may need to change to change the path name for mozilla,
# shown in the function get_seamonkey_engine_command, line 529.


use strict;
use Getopt::Mixed "nextOption";

my $os_type = &get_os_type;
my $unixish = (($os_type ne "WIN") && ($os_type ne "MAC"));
my $path_sep = ($os_type eq "MAC") ? ":" : "/";
my $redirect_command = ($os_type ne "MAC") ? " >& " : "";

# command line option defaults
my $opt_suite_path = "";
my $opt_trace = 0;
my $opt_classpath = "";
my $opt_seamonkey_opt = 0;
my $opt_seamonkey_ms = 0;
my @opt_engine_list;
my $opt_engine_type = "";
my $opt_engine_params = "";
my $opt_user_output_file = 0;
my $opt_output_file = "";
my @opt_test_list_files;
my @opt_neg_list_files;
my $opt_shell_path = "";
my $opt_java_path = "";
my $opt_bug_url = "http://bugzilla.mozilla.org/show_bug.cgi?id=";
my $opt_console_failures = 0;
my $opt_lxr_url = "http://lxr.mozilla.org/mozilla/source/js/tests/";
my $opt_exit_munge = ($os_type ne "MAC") ? 1 : 0;

# command line option definition
my $options = "b=s bugurl>b c=s classpath>c e=s engine>e f=s file>f " .
  "h help>h i j=s javapath>j k confail>k l=s list>l L=s neglist>L " .
  "o=s opt>o p=s testpath>p s=s shellpath>s t trace>t u=s lxrurl>u " .
  "x noexitmunge>x";


#if ($os_type eq "MAC") {
#    $opt_suite_path = `directory`;
#    $opt_suite_path =~ s/[\n\r]//g;
#    $opt_suite_path .= ":";
#} else {
#    $opt_suite_path = "file://";
#}

&parse_args;

my $user_exit = 0;
my ($engine_command, $html, $failures_reported, $tests_completed,
    $exec_time_string); 
my @failed_tests;
my @test_list = &get_test_list;

if ($#test_list == -1) {
    die ("Nothing to test.\n");
}

if ($unixish) {
    # on unix, ^C pauses the tests, and gives the user a chance to quit but 
    # report on what has been done, to just quit, or to continue (the
    # interrupted test will still be skipped.)
    # windows doesn't handle the int handler they way we want it to,
    # so don't even pretend to let the user continue.
    $SIG{INT} = 'int_handler';
}

&main;

#End.

sub main {
    my $start_time;
    my $pid;

    while ($opt_engine_type = pop (@opt_engine_list)) {
        dd ("Testing engine '$opt_engine_type'");

        $engine_command = &get_engine_command;
        $html = "";
        @failed_tests = ();
        $failures_reported = 0;
        $tests_completed = 0;
        $start_time = time;


        &execute_tests (@test_list);

        my $exec_time = (time - $start_time);
        my $exec_hours = int($exec_time / 60 / 60);
        $exec_time -= $exec_hours * 60 * 60;
        my $exec_mins = int($exec_time / 60);
        $exec_time -= $exec_mins * 60;
        my $exec_secs = ($exec_time % 60);

        if ($exec_hours > 0) {
            $exec_time_string = "$exec_hours hours, $exec_mins minutes, " .
              "$exec_secs seconds";
        } elsif ($exec_mins > 0) {
            $exec_time_string = "$exec_mins minutes, $exec_secs seconds";
        } else {
            $exec_time_string = "$exec_secs seconds";
        }

        if (!$opt_user_output_file) {
            $opt_output_file = &get_tempfile_name;
        }
    
        &write_results;
        
    }
    $pid=fork;
    if($pid == 0) {
      exec("netscape file:/u/jtaylor/TestSuite/" . $opt_output_file);
    }
    if($pid != 0) {
      exit(0);
    }
    exec("netscape file:/u/jtaylor/TestSuite/" . $opt_output_file);
    exit(0);
}

sub execute_tests {
    my (@test_list) = @_;
    my ($test, $shell_command, $line, @output);
    my $file_param = "";
    my ($last_suite, $last_test_dir);
    my $failure_lines;
    my $pid;
    my $numticks=0;
    my $count=1;

    &status ("Executing " . ($#test_list + 1) . " test(s).");

    foreach $test (@test_list) {
        my ($suite, $test_dir, $test_file) = split($path_sep, $test);
        # *-n.js is a negative test, expect exit code 3 (runtime error)
        my $expected_exit = ($test =~ /\-n\.js$/) ? 3 : 0;
        my ($got_exit, $exit_signal);
        my $bug_line;

	@output = ();

        # user selected [Q]uit from ^C handler.
        if ($user_exit) {
            return;
        }
        
	$shell_command=$engine_command;

	######################
	&dd("Runing test $count of " . ($#test_list + 1));
	$count ++;

	$pid = fork;	
	if($pid != 0) {
	  $numticks = 0;
	  sleep 1;
	  open (OUTPUT, "mozilla-output.txt");
	  while (<OUTPUT> && $numticks<8) {
	    push(@output, <OUTPUT>);
	    sleep 10;
	    $numticks++;
	  }
	  if($numticks >= 8) {
	    @output = ();
	    push(@output, "\nFailed: Timeout error or infinite loop\n");
	  }

	  close(OUTPUT);

	  kill('KILL', $pid);
	  &killall ("mozilla");
	  #exec("kill -9 mozilla-bin");
	  
	  #&dd ("got output:\n" . join ("", @output) . "\nso far.");

	} else  {
	  &dd ("executing: " . $shell_command . $file_param .
	       $test);
	  exec($shell_command . $file_param . $test . $redirect_command .
	       " mozilla-output.txt");
	  
	}

	waitpid($pid, 0);

	#&dd("Test results:\n-----------\n");
	#@output = grep(/^&&/, @output);
	#&dd(@output);


        if ($opt_exit_munge == 1) {
            # signal information in the lower 8 bits, exit code above that
            $got_exit = ($? >> 8);
            $exit_signal = ($? & 255);
        } else {
            # user says not to munge the exit code
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
                &dd ("Test case expects exit code $expected_exit");
            }

            # watch for failures
            if ($line =~ /error/i && !($line =~ /widget/i) && !($line =~ /Security/i) && !($line =~ /JavaScript/i)&& !($line =~ /ERROR_DOM/i) && !($line =~ /Access/i)) {
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

    unlink 'mozilla-output.txt';
}

sub write_results {
    my ($list_name, $neglist_name);
    my $completion_date = localtime;
    my $failure_pct = int(($failures_reported / $tests_completed) * 10000) /
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
    my ($option, $value, $lastopt);
    
    &dd ("checking command line options.");
    
    Getopt::Mixed::init ($options);
    $Getopt::Mixed::order = $Getopt::Mixed::RETURN_IN_ORDER;
    
    while (($option, $value) = nextOption()) {

        if ($option eq "b") {
            &dd ("opt: setting bugurl to '$value'.");
            $opt_bug_url = $value;
            
        }  elsif (($option eq "e") || (($option eq "") && ($lastopt eq "e"))) {
            &dd ("opt: adding engine $value.");
            push (@opt_engine_list, $value);

        } elsif ($option eq "f") {
            if (!$value) {
                die ("Output file cannot be null.\n");
            }
            &dd ("opt: setting output file to '$value'.");
            $opt_user_output_file = 1;
            $opt_output_file = $value;
            
        } elsif ($option eq "h") {
            &usage;

        }  elsif ($option eq "k") {
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

        } elsif ($option eq "o") {
            $opt_engine_params = $value;
            &dd ("opt: setting engine params to '$opt_engine_params'.");
            
        } elsif ($option eq "p") {
            $opt_suite_path = $value;
            if (!($opt_suite_path =~ /[\/\\]$/)) {
                $opt_suite_path .= "/";
            }
            &dd ("opt: setting suite path to '$opt_suite_path'.");
            
        } elsif ($option eq "s") {
            $opt_shell_path = $value;
            &dd ("opt: setting shell path to '$opt_shell_path'.");
            
        } elsif ($option eq "t") {
            &dd ("opt: tracing output.  (console failures at no extra charge.)");
            $opt_console_failures = 1;
            $opt_trace = 1;
            
        } elsif ($option eq "u") {
            &dd ("opt: setting lxr url to '$value'.");
            $opt_lxr_url = $value;
            
        } elsif ($option eq "x") {
            &dd ("opt: turning off exit munging.");
            $opt_exit_munge = 0;
            
        } else {
            &usage;
        }

        $lastopt = $option;

    }
    
    Getopt::Mixed::cleanup();
    
    if ($#opt_engine_list == -1) {
        die "You must select a shell to test in.\n";
    }

}

#
# print the arguments that this script expects
#
sub usage {
    print STDERR 
      ("\nusage: $0 [<options>] \n" .
       "(-b|--bugurl)             Bugzilla URL.\n" .
       "                          (default is $opt_bug_url)\n" .
       "(-e|--engine) <type> ...  Specify the type of engine(s) to test.\n" .
       "                          <type> is one or more of\n" .
       "                          (seamonkey).\n" .
       "(-f|--file) <file>        Redirect output to file named <file>.\n" .
       "                          (default is " .
       "results-<engine-type>-<date-stamp>.html)\n" .
       "(-h|--help)               Print this message.\n" .
       "(-k|--confail)            Log failures to console (also.)\n" . 
       "(-l|--list) <file> ...    List of tests to execute.\n" . 
       "(-L|--neglist) <file> ... List of tests to skip.\n" . 
       "(-o|--opt) <options>      Options to pass to the JavaScript engine.\n" .
       "                          (Make sure to quote them!)\n" .
       "(-p|--testpath) <path>    Root of the test suite. (default is ./)\n" .
       "(-s|--shellpath) <path>   Location of JavaScript shell.\n" .
       "(-t|--trace)              Trace script execution.\n" .
       "(-u|--lxrurl) <url>       Complete URL to tests subdirectory on lxr.\n" .
       "                          (default is $opt_lxr_url)\n" .
       "(-x|--noexitmunge)        Don't do exit code munging (try this if it\n" .
       "                          seems like your exit codes are turning up\n" .
       "                          as exit signals.)\n");
    exit (1);
    
}

#
# get the shell command used to start the (either) engine
#
sub get_engine_command {
    
    my $retval;
    
    if ($opt_engine_type eq "seamonkey") {
        &dd ("getting sea monkey engine command.");
        $opt_seamonkey_opt = 0;
        $opt_seamonkey_ms = 0;
        $retval = &get_seamonkey_engine_command;
    }  else {
        die ("Unknown engine type selected, '$opt_engine_type'.\n");
    }
    
    $retval .= " $opt_engine_params";

    &dd ("got '$retval'");
    
    return $retval;
    
}

#
# use the seamonkey browser to test
#
sub get_seamonkey_engine_command {   
    my $retval;

    #Modify line for local machine
    $retval = "/usr/mozilla/package/mozilla ";

}

sub get_os_type {

    if ("\n" eq "\015") {
        return "MAC";
    }

    my $uname = `uname -a`;
    
    if ($uname =~ /WIN/) {
        $uname = "WIN";
    } else {
        chop $uname;
    }
    
    &dd ("get_os_type returning '$uname'.");
    return $uname;
    
}

sub get_test_list {
    my @test_list;
    my @neg_list;

    if ($#opt_test_list_files > -1) {
        my $list_file;

        &dd ("getting test list from user specified source.");

        foreach $list_file (@opt_test_list_files) {
            push (@test_list, expand_user_test_list($list_file));
        }
    } else {
        &dd ("no list file, groveling in '$opt_suite_path'.");

        @test_list = &get_default_test_list($opt_suite_path);
    }

    if ($#opt_neg_list_files > -1) {
        my $list_file;
        my $orig_size = $#test_list + 1;
        my $actually_skipped;

        &dd ("getting negative list from user specified source.");

        foreach $list_file (@opt_neg_list_files) {
            push (@neg_list,  expand_user_test_list($list_file));
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
    my ($list_file) = @_;
    my @retval = ();

    if ($list_file =~ /\.html$/ || -d $list_file) {

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
    my ($entry) = @_;
    my @retval;

    if ($entry =~ /\.*$|/) {
        # it's a regular entry, add it to the list
        push (@retval, $entry);     
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
    my ($suite_path) = @_;
    my @suite_list = &get_subdirs($suite_path);
    my $suite;
    my @retval;

    foreach $suite (@suite_list) {
        my @test_dir_list = get_subdirs ($suite_path . $suite);
        my $test_dir;
        
        foreach $test_dir (@test_dir_list) {
            my @test_list = get_js_files ($suite_path . $suite . $path_sep .
                                          $test_dir);
            my $test;
            
            foreach $test (@test_list) {
                $retval[$#retval + 1] = $suite . $path_sep . $test_dir . $path_sep . $test;
            }
        }
    }
    
    return @retval;
    
}

#
# generate an output file name based on the date
#
sub get_tempfile_name {
    my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) =
      &get_padded_time (localtime);
    my $rv;
    
    if ($os_type ne "MAC") {
        $rv = "results-" . $year . "-" . $mon . "-" . $mday . "-" . $hour . 
          $min . $sec . "-" . $opt_engine_type;
    } else {
        $rv = "res-" . $year . $mon . $mday . $hour . $min . $sec . "-" .
          $opt_engine_type
    }
    
    return $rv . ".html";
}

sub get_padded_time {
    my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = @_;
    
    $mon++;
    $mon = &zero_pad($mon);
    $year += 1900;
    $mday= &zero_pad($mday);
    $sec = &zero_pad($sec);
    $min = &zero_pad($min);
    $hour = &zero_pad($hour);
    
    return ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst);
    
}

sub zero_pad {
    my ($string) = @_;
    
    $string = ($string < 10) ? "0" . $string : $string;
    return $string;
}

sub subtract_arrays {
    my ($whole_ref, $part_ref) = @_;
    my @whole = @$whole_ref;
    my @part = @$part_ref;
    my $line;

    foreach $line (@part) {
        @whole = grep (!/$line/, @whole); 
    }

    return @whole;

}

#
# Convert unix path to mac style.
#
sub unix_to_mac {
    my ($path) = @_;
    my @path_elements = split ("/", $path);
    my $rv = "";
    my $i;

    foreach $i (0 .. $#path_elements) {
        if ($path_elements[$i] eq ".") {
            if (!($rv =~ /\:$/)) {
                $rv .= ":";
            }
        } elsif ($path_elements[$i] eq "..") {
            if (!($rv =~ /\:$/)) {
                $rv .= "::";
            } else {
                $rv .= ":";
            }
        } elsif ($path_elements[$i] ne "") {
            $rv .= $path_elements[$i] . ":";
        }

    }

    $rv =~ s/\:$//;
       
    return $rv;
}

#
# Convert unix path to correct style based on platform.
#
sub xp_path {
    my ($path) = @_;

    if ($os_type eq "MAC") {
        return &unix_to_mac($path);
    } else {
        return $path;
    }
}

#
# given a directory, return an array of all subdirectories
#
sub get_subdirs {
    my ($dir)  = @_;

    my @subdirs;
    
    if ($os_type ne "MAC") {
        if (!($dir =~ /\/$/)) {
            $dir = $dir . "/";
        }
    } else {
        if (!($dir =~ /\:$/)) {
            $dir = $dir . ":";
        }
    }        
        
    opendir (DIR, $dir) || die ("couldn't open directory $dir: $!");
    my @testdir_contents = readdir(DIR);
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
    my ($test_subdir) = @_;
    my (@js_file_array, @subdir_files);
    
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

#
# creats report info for an individual failed report
#
sub report_failure {
    my ($test, $message, $bug_line) = @_;

    my $id;
    my $lft = "/";

    $failures_reported++;

    $message =~ s/\n+/\n/g;

    $id = $test;
    $id =~ s/\.\D*$//;
    $id =~ s/^\D*//;

     if ($opt_console_failures) {
        print STDERR ("*-* Testcase $test failed:\n$message\n");
    }

    $message =~ s/\n/<br>\n/g;

    $html .= "<a name='failure$failures_reported'></a>";
    if ($opt_lxr_url) {

        $html .= "<dd><b>".
          "Testcase <a target='other_window' href='$test'>$test</a> " .
            "failed</b> |  Bugzilla id# <a href='$opt_bug_url$id'>$id</a><br>\n";
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

sub killall {
  my ($name) = @_;

  open (PIDS, "ps xa | grep $name |");
  my @pidlist = <PIDS>;

  foreach (@pidlist) {
    /(\d+)/;
    my $pid = $1;
    kill ('KILL', $1)
  }
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
    my $resp;

    do {
        print ("\n*** User Break: Just [Q]uit, Quit and [R]eport, [C]ontinue ?");
        $resp = <STDIN>;
    } until ($resp =~ /[QqRrCc]/);

    if ($resp =~ /[Qq]/) {
        print ("User Exit.  No results were generated.\n");
        exit;
    } elsif ($resp =~ /[Rr]/) {
        $user_exit = 1;
    }

}
