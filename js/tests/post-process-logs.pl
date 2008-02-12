#!/usr/bin/perl -w
# -*- Mode: Perl; tab-width: 4; indent-tabs-mode: nil; -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla JavaScript Testing Utilities
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary <bclary@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

use Getopt::Mixed "nextOption";
use File::Temp qw/ tempfile tempdir /;
use File::Basename;

sub dbg;
sub outresults;
sub outputrecord;

local $file;
local $temp;
my $debug = $ENV{DEBUG};

(undef, $temp) = tempfile();

open TEMP, ">$temp" or
    die "FATAL ERROR: Unable to open temporary file $temp for writing: $!\n";

local ($test_id, 
       $test_result, 
       $test_type, 
       $tmp_test_type,
       $test_description, 
       @messages,
       $test_processortype, 
       $test_kernel, 
       $test_suite,
       $exit_flag,
       $exit_status,
       $tmp_exit_flag,
       $tmp_exit_status,
       $page_flag, 
       $page_status,
       $tmp_page_status,
       $tmp_page_flag,
       $test_state);

local ($actual_exit, $actual_signal);
local $count_records = 0;
local $count_jstests = 0;

while ($file = shift @ARGV)
{
    @messages = ();

    dbg "file: $file";

    my $filename = basename $file;

    dbg "filename: $file";

    local ($test_date, $test_product, $test_branchid, $test_buildtype,
           $test_os,
           $test_machine,$test_global_target) = split /,/, $filename;

    $test_branchid =~ s/[^0-9.]//g;
    $test_global_target =~ s/.log$//;

    local ($test_timezone) = $test_date;
    $test_timezone =~ s/.*([-+]\d{4,4})/$1/;

#    dbg "test_date:          $test_date";
#    dbg "test_timezone:      $test_timezone";
#    dbg "test_product:       $test_product";
#    dbg "test_branchid:      $test_branchid";
#    dbg "test_buildtype:     $test_buildtype";
#    dbg "test_os:            $test_os";
#    dbg "test_machine:       $test_machine";
#    dbg "test_suite: $test_suite";

    open FILE, "$file" or die "FATAL ERROR: unable to open $file for reading: $!\n";

    dbg "process header with environment variables used in test";

    while (<FILE>)
    {
        chomp;

        # remove carriage returns, bels and other annoyances.
        $_ =~ s/[\r]$//;
        $_ =~ s/[\r]/CR/g;
        $_ =~ s/[\x01-\x08]//g;
        $_ =~ s/\s+$//;

        dbg  "INPUT: $_";

        last if ( $_ =~ /^environment: EOF/);

        if (($envvar, $envval) = $_ =~ /^environment: (TEST_[A-Z0-9_]*)=(.*)/ )
        {
            dbg "envvar=$envvar, envval=$envval";
            $envvar =~ tr/A-Z/a-z/;
            $$envvar = $envval;
            dbg $envvar . "=" . $$envvar;
        }
        elsif (($envval) = $_ =~ /^environment: OSID=(.*)/ )
        {
            $test_os = $envval;
        }
    }

    if ($test_product eq "js")
    {
        while (<FILE>)
        {
            chomp;

            dbg  "INPUT: $_";

            $_ =~ s/[\r]$//;
            $_ =~ s/[\r]/CR/g;
            $_ =~ s/[\x01-\x08]//g;
            $_ =~ s/\s+$//;

            next if ( $_ !~ /^jstest: /);

            ++$count_jstests;

            ($test_id)          = $_ =~ /^jstest: (.*?) *bug:/;
            ($test_result)      = $_ =~ /result: (.*?) *type:/;
            ($test_type)        = $_ =~ /type: (.*?) *description:/;
            ($test_description) = $_ =~ /description: (.*)/;

            if (!$test_description)
            {
                $test_description = "";
            }
            else
            {
                ($actual_exit, $actual_signal) = $test_description =~ /expected: Expected exit [03] actual: Actual exit ([0-9]*), signal ([0-9]*)/;
                if (defined($actual_exit) or defined($actual_signal))
                {
                    if ($actual_exit > 3 || $actual_signal > 0)
                    {
                        $test_description =~ s/ *expected: Expected exit [03] actual: Actual exit ([0-9]*), signal ([0-9]*) /EXIT STATUS: CRASHED $actual_exit signal $actual_signal, /;
                    }
                }
                elsif ($test_result eq "FAILED TIMED OUT")
                {
                    $test_description = "EXIT STATUS: TIMED OUT, $test_description";
                    $test_result = "FAILED";
                }
            }

            dbg "test_id:          $test_id";
            dbg "test_result:      $test_result";
            dbg "test_type:        $test_type";
            dbg "test_description: $test_description";

            outputrecord;

            dbg "-";

        }
    }

    elsif ($test_product eq "firefox")
    {
        $test_state     = 'started program';
        $test_buildtype = "nightly" unless $test_buildtype;
        $test_type      = "browser";
        @messages       = ();

        dbg "skip over test run start up";

        while (<FILE>)
        {
            chomp;

            # remove carriage returns, bels and other annoyances.
            $_ =~ s/[\r]$//;
            $_ =~ s/[\r]/CR/g;
            $_ =~ s/[\x01-\x08]//g;
            $_ =~ s/\s+$//;

            dbg  "INPUT: $_";

            last if (/^Spider: Start/ && !/start-spider.html/);
        }

        dbg "begin processing test results";

        while (<FILE>)
        {
            chomp;

            # remove carriage returns, bels and other annoyances.
            $_ =~ s/[\r]$//;
            $_ =~ s/[\r]/CR/g;
            $_ =~ s/[\x01-\x08]//g;
            $_ =~ s/\s+$//;

            if ($debug)
            {
                dbg "INPUT: $_";
                dbg "test_id: $test_id, test_state: $test_state, page_status: $page_status";
            }

            # massage the input to make more uniform across test types and platforms
            s/\.js, line ([0-9]*): out of memory/.js:$1: out of memory/g;

            if ( /^Spider: Begin loading.*urllist/)
            {
                $test_state = 'loading list'; # optional, only if run in non-restart mode
                dbg "ignore loading the test list when running in non restart mode"
            }
            elsif ( ($tmp_test_id) = $_ =~ /^Spider: Begin loading http.*test=([^;]*);/)
            {
                dbg "Begin loading $tmp_test_id";

                die "FATAL ERROR: Spider Begin loading: previous test not completed: test: $test_id, current test: $tmp_test_id, test state: $test_state, log: $file" 
                    if ("started program, loading list, finished test, exited program" !~ /$test_state/);

                $test_state  = 'loading test';
                $test_id     = $tmp_test_id;
                $page_flag   = '';
                $page_status = '';
                $exit_flag   = '';
                $exit_status = '';
                @messages    = ();
            }
            elsif ( ($tmp_test_id) = $_ =~ /^JavaScriptTest: Begin Test (.*)/)
            {
                dbg "Begin Test: test: $test_id, current test: $tmp_test_id";

                die "FATAL ERROR: JavaScript Begin Test: test mismatch: test $test_id, current test $tmp_test_id, log: $file" 
                    if ($test_id ne $tmp_test_id);

                die "FATAL ERROR: JavaScript Begin Test: test not loaded: test $test_id, current test $tmp_test_id, test state: $test_state, log: $file" 
                    if ("loading test" !~ /$test_state/);

                $test_state = 'running test';
            }
            elsif ( $_ =~ /^jstest: /)
            {
                dbg "processing jstest";

                # may be in 'completed test' for delayed browser only tests.
                die "FATAL ERROR: jstest not in test: test state: $test_state: $_, log: $file" 
                    if ('running test, reporting test, completed test' !~ /$test_state/);

                $test_state         = 'reporting test';
                ($tmp_test_id)      = $_ =~ /^jstest: (.*?) *bug:/;
                ($test_result)      = $_ =~ /result: (.*?) *type:/;
                ($tmp_test_type)    = $_ =~ /type: (.*?) *description:/;
                ($test_description) = $_ =~ /description: (.*)/;

                if (!$test_description)
                {
                    $test_description = "";
                }

                ++$count_jstests;

                if ($debug)
                {
                    dbg "tmp_test_id:      $tmp_test_id";
                    dbg "test_result:      $test_result";
                    dbg "tmp_test_type:    $tmp_test_type";
                    dbg "test_description: $test_description";
                }

                die "FATAL ERROR: jstest test type mismatch: start test_type: $test_type, current test_type: $tmp_test_type, test state: $test_state, log: $file" 
                    if ($test_type ne $tmp_test_type);

                if ($test_id ne $tmp_test_id)
                {
                    # a previous test delayed its output either due to buffering or another reason
                    # recover by temporarily setting the test id to the previous test and outputing 
                    # the record.
                    warn "WARNING: jstest test id mismatch: start test_id: $test_id, current test_id: $tmp_test_id, test state: $test_state, log: $file";
                    my $save_test_id = $test_id;
                    $test_id = $tmp_test_id;
                    outputrecord;
                    $test_id = $save_test_id;
                }
                else
                {
                    outputrecord;
                }
            }
            elsif ( $test_state ne 'loading list' && (($tmp_page_flag, $tmp_page_status) = $_ =~ /(http:[^:]*): PAGE STATUS: (.*)/) )
            {
                $page_flag = $tmp_page_flag;
                $page_status = $tmp_page_status;

                dbg "Processing PAGE STATUS: test_state: $test_state, page_flag: $page_flag, page_status: $page_status";

                die "FATAL ERROR: Test loaded but not in a test: $test_state: $_, log: $file" 
                    if ('loading test, running test, reporting test' !~ /$test_state/);
                    
                ($tmp_test_id) = $page_flag =~ /http.*test=([^;]*);/;

                die "FATAL ERROR: Test loaded does not match currently running test: test: $test_id, current test: $tmp_test_id, log: $file"
                    if ($test_id ne $tmp_test_id);

                if ($page_status =~ /^NORMAL/)
                {
                    # since the deferred execution browser based tests finish loading before the
                    # tests executes, we must wait to handle the case where the test may have
                    # failed report any test results
                }
                elsif ($page_status =~ /^TIMED OUT/)
                {
                    # if we got here, the test terminated because it
                    # is one of the old style -n.js negative tests, it
                    # crashed or the harness messed up.

                    dbg "Test $test_id timed out";

                    $test_description = join '; ', @messages;
                    $test_result      = "FAILED";
                    $test_description = "EXIT STATUS: $page_status, $test_description";

                    outputrecord;

                    $test_state = 'completed test';
                    @messages   = ();

                }
                else
                {
                    die "FATAL ERROR: Invalid Page Status: $page_status, log: $file";
                }

            }
            elsif ( ($tmp_test_id) = $_ =~ /^JavaScriptTest: End Test (.*)/)
            {
                dbg "End test test_id: $test_id, tmp_test_id: $tmp_test_id, page_status: $page_status, test_state: $test_state";

                die "FATAL ERROR: JavaScript End Test: test mismatch: test $test_id, current test $tmp_test_id, log: $file" 
                    if ($test_id ne $tmp_test_id);
    
                die "FATAL ERROR: JavaScript End Test: not in a test: test $test_id, current test $tmp_test_id, test state: $test_state, log: $file" 
                    if ('reporting test, running test, completed test' !~ /$test_state/);

                if ($page_status && $page_status =~ /^NORMAL/)
                {
                    dbg "Test $test_id loaded normally before the test was executed.";

                    if ($test_state eq 'running test')
                    {
                        # if we got here, the test completed loading but it
                        # didn't report a result before End Test was seen.

                        dbg "Test $test_id did not report any results";

                        # XXX: too much trouble updating failure list for the moment
                        #      to make this change.
                        # $test_description = 'No test results reported. ' . (join '; ', @messages);
                        $test_description = (join '; ', @messages);

                        if ($test_id =~ /-n.js$/ && $test_description =~
                            /JavaScript (Exception|Error)/i)
                        {
                            # force -n.js tests which pass if they end
                            # with an error or exception to 'pass'
                            $test_result = "PASSED";
                        }
                        else
                        {
                            $test_result = "FAILED";
                        }

                        outputrecord;
                    }
                }

                $test_state = 'finished test';
            }
            elsif ( ($tmp_exit_flag, $tmp_exit_status) = $_ =~ /(http:[^:]*): EXIT STATUS: (.*)/ )
            {
                $exit_flag = $tmp_exit_flag,
                $exit_status = $tmp_exit_status;

                dbg "Processing EXIT STATUS: exit_flag: $exit_flag, exit_status: $exit_status";

                if ($exit_status =~ /^NORMAL/)
                {
                    dbg "Program exited normally";
                    
                    die "FATAL ERROR: Program exited normally without running tests: test state: $test_state, $_, log: $file"
                        if ('started program, loading list' =~ /$test_state/);

                    die "FATAL ERROR: Program exited normally without finishing last test: test state: $test_state, $_, log: $file"
                        if ($test_state ne 'finished test');
                }
                else
                {
                    dbg "Program exited abnormally: test state: $test_state";

                    if ($test_state eq 'finished test' && $exit_status =~ /^TIMED OUT/)
                    {
                        # as long as the program time out is greater than the page time out
                        # this will have been reported already by the End Test page status code
                    }
                    else
                    {
                        $test_result = "FAILED";
                        $test_description = join '; ', @messages;

                        if ($test_state eq 'running test')
                        {
                            $test_description = "No test results reported. $test_description";
                        }
                        $test_description = "EXIT STATUS: $exit_status, $test_description";
                        outputrecord;
                    }
                }

                $test_state = 'exited program';
                @messages = ();

            }
            elsif (!/^[-+]{2,2}(WEBSHELL|DOMWINDOW)/ && !/^Spider:/ && !/^JavaScriptTest:/ && !/^WARNING:/)
            {
                dbg "Saving message $_";
                push @messages, ($_);
            }
        }
    }
    close FILE;

    undef $test_branchid;
    undef $test_date;
    undef $test_buildtype;
    undef $test_machine;
    undef $test_product;
    undef $test_suite;
}

close TEMP;

outresults;

unlink $temp;

if ($debug)
{
    dbg "jstests       $count_jstests\n";
    dbg "total records $count_records\n";
}

sub dbg {
    if ($debug)
    {
        my $msg = shift;
        print STDERR "DEBUG: $msg\n";
    }
}

sub outresults
{
    system("sort < $temp | uniq");
}

sub outputrecord
{
    ++$count_records;

    # cut off the extra jstest: summaries as they duplicate the other
    # output and follow it.
    $test_description =~ s/jstest:.*//;

    if (length($test_description) > 6000)
    {
        $test_description = substr($test_description, 0, 6000);
    }

    my $output = 
        "TEST_ID=$test_id, TEST_BRANCH=$test_branchid, TEST_RESULT=$test_result, " .
        "TEST_BUILDTYPE=$test_buildtype, TEST_TYPE=$test_type, TEST_OS=$test_os, " . 
        "TEST_MACHINE=$test_machine, TEST_PROCESSORTYPE=$test_processortype, " . 
        "TEST_KERNEL=$test_kernel, TEST_DATE=$test_date, TEST_TIMEZONE=$test_timezone, " . 
        "TEST_DESCRIPTION=$test_description\n";

    if ($debug)
    {
        dbg "RECORD: $output";
    }
    print TEMP $output;

}
