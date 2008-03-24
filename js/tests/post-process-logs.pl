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

# required for mac os x 10.5 to prevent sort from 
# complaining about illegal byte sequences
$ENV{LC_ALL} = 'C';

(undef, $temp) = tempfile();

open TEMP, ">$temp" or
    die "FATAL ERROR: Unable to open temporary file $temp for writing: $!\n";

local ($test_id,
       $tmp_test_id,
       %test_id,
       %test_reported,
       $test_result, 
       $test_type, 
       $tmp_test_type,
       $test_description, 
       @messages,
       $test_processortype, 
       $test_kernel, 
       $test_suite,
       $exit_status,
       $page_status,
       $state);

local ($actual_exit, $actual_signal);

local %test_reported       = ();

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

    open FILE, "$file" or die "FATAL ERROR: unable to open $file for reading: $!\n";

    dbg "process header with environment variables used in test";

    while (<FILE>)
    {
        $state = 'failure';

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

            if (/Wrote results to/)
            {
                $state = 'success';
                last;
            }

            $_ =~ s/[\r]$//;
            $_ =~ s/[\r]/CR/g;
            $_ =~ s/[\x01-\x08]//g;
            $_ =~ s/\s+$//;

            next if ( $_ !~ /^jstest: /);

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

            if ($test_description =~ /error: can.t allocate region/ || /set a breakpoint in malloc_error_break/ || 
                /set a breakpoint in szone_error to debug/ || /malloc:.*mmap/ || /vm_allocate/ )
            {
                dbg "Adding message: /$test_id:0: out of memory";
                $test_description .= "; /$test_id:0: out of memory";
            }

            dbg "test_id:          $test_id";
            dbg "test_result:      $test_result";
            dbg "test_type:        $test_type";
            dbg "test_description: $test_description";

            outputrecord $test_id, $test_description, $test_result;

            dbg "-";
        }
    }
    elsif ($test_product eq "firefox")
    {
        %test_id         = ();
        @messages        = ();

        $page_status     = '';
        $exit_status     = '';
        $test_buildtype  = "nightly" unless $test_buildtype;
        $test_type       = "browser";


# non-restart mode. start spider; for each test { load test;} exit spider;
# restart mode.     for each test; { start spider; load test; exit spider; }
# 
#                    Expected sequence if all output written to the log.
#
#    Input                                            Initial State       Next State       userhook event                         outputrecord
# ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Spider: Start.*start-spider.html                    idle                startrun
# Spider: Begin loading.*start-spider.html            startrun            startrun
# Start Spider: try.*EXIT STATUS: NORMAL              startrun            initialized
# Start Spider: try.*EXIT STATUS: (TIMED OUT|CRASHED) startrun            startrun
# Spider: Start.*urllist                              initialized         initialized      (non restart mode)
# Spider: Begin loading.*urllist                      initialized         initialized      (non restart mode)
# Spider: Finish loading.*urllist                     initialized         initialized      (non restart mode)
# Spider: Current Url:.*urllist                       initialized         initialized      (non restart mode)
# Spider: Start.*test=t;                              initialized         starttest        (has test id)
# JavaScriptTest: Begin Run                           starttest           starttest        onStart
# Spider: Begin loading.*test=t;                      starttest           loadingtest      (has test id)
# JavaScriptTest: Begin Test t;                       loadingtest         runningtest      onBeforePage (has test id)                         
# jstest: t                                           runningtest         reportingtest    (has test id)                          yes.        
# Spider: Finish loading.*t=t;                        reportingtest       loadedtest       (has test id)                                      
# Spider: Finish loading.*t=t;                        runningtest         pendingtest      (has test id)                                      
# Spider: Current Url:.*test=t;                       loadedtest          loadedtest       (has test id)                                      
# http://.*test=t;.*PAGE STATUS: NORMAL               loadedtest          loadedtest       onAfterPage (has test id)                          
# http://.*test=t;.*PAGE STATUS: TIMED OUT            loadedtest          endrun           onPageTimeout (has test id)            yes.        
# JavaScriptTest: t Elapsed time                      loadedtest          completedtest    checkTestCompleted (has test id)                   
# JavaScriptTest: End Test t                          completedtest       completedtest    checkTestCompleted (has test id)                   
# JavaScriptTest: End Test t                          endrun              endrun           onPageTimeout (has test id)                        
# Spider: Start.*test=t;                              completedtest       starttest        (non restart mode) (has test id)
# JavaScriptTest: End Run                             completedtest       endrun           onStop
# JavaScriptTest: End Run                             loadedtest          endrun           onStop
# Spider: Start.*test=t;                              endrun              starttest        (restart mode) (has test id)
# http://.*test=t;.*EXIT STATUS: NORMAL               endrun              endrun           (has test id)                          maybe.
# http://.*test=t;.*EXIT STATUS: TIMED OUT            endrun              endrun           (has test id)                          yes.
# http://.*test=t;.*EXIT STATUS: CRASHED              endrun              endrun           (has test id)                          yes.
# /work/mozilla/mozilla.com/test.mozilla.com/www$     endrun              success
# EOF                                                 success             success
# EOF                                                 endrun              failure
# 
# States        has test id
# -------------------------
# idle
# startrun
# initialized
# starttest     has test id
# loadingtest   has test id
# runningtest   has test id
# pendingtest   has test id
# reportingtest has test id
# loadedtest    has test id
# endrun        has test id
# completedtest has test id
# success
# failure

        dbg "Assuming starting in restart mode";

        $mode  = 'restart';
        $state = 'idle';

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
                dbg "\nINPUT: $_";
            }

            # massage the input to make more uniform across test types and platforms
            s/\.js, line ([0-9]*): out of memory/.js:$1: out of memory/g;


            if (/^Spider: Start.*start-spider.html/)
            {
                if ($state eq 'idle')
                {
                    $state = 'startrun';
                }
                else
                {
                    warn "WARNING: state: $state, expected: idle, log: $file";
                    $state = 'startrun';
                }
            }
            elsif (/^Spider: Begin loading.*start-spider.html/)
            {
                if ($state eq 'startrun')
                {
                    $state = 'startrun';
                }
                else
                {
                    warn "WARNING: state: $state, expected: startrun, log: $file";
                    $state = 'startrun';
                }
            }
            elsif (/^Start Spider: try.*EXIT STATUS: NORMAL/)
            {
                if ($state eq 'startrun')
                {
                    $state = 'initialized';
                }
                else
                {
                    warn "WARNING: state: $state, expected: startrun, log: $file";
                    $state = 'initialized';
                }
            }
            elsif (/^Start Spider: try.*EXIT STATUS: (TIMED OUT|CRASHED)/)
            {
                if ($state eq 'startrun')
                {
                    $state = 'startrun';
                }
                else
                {
                    warn "WARNING: state: $state, expected: startrun, log: $file";
                    $state = 'startrun';
                }
            }
            elsif ( /^Spider: Start: -url .*test.mozilla.com.tests.mozilla.org.js.urllist-/)
            {
                dbg "Setting mode to nonrestart";

                $mode = 'nonrestart';

                if ($state eq 'initialized')
                {
                    $state = 'initialized';
                }
                elsif ($state eq 'starttest')
                {
                    $state = 'initialized';
                }
                else
                {
                    warn "WARNING: state: $state, expected: initialized, starttest, log: $file";
                    $state = 'initialized';
                }
            }
            elsif ( ($tmp_test_id) = $_ =~ /^Spider: Start.*http.*test=([^;]*);/)
            {
                if ($state eq 'initialized')
                {
                    $state = 'starttest';
                }
                elsif ($state eq 'completedtest')
                {
                    $state = 'starttest';
                }
                elsif ($state eq 'endrun')
                {
                    $state = 'starttest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: initialized, completedtest, endrun, log: $file";
                    $state = 'starttest';
                }

                $test_id{$state} = $tmp_test_id;
                $test_id{'loadingtest'} = $test_id{'runningtest'} = $test_id{'reportingtest'} = $test_id{'loadedtest'} = $test_id{'endrun'} = $test_id {'completedtest'} = $test_id{'loadedtest'} = '';
                @messages = ();
            }
            elsif ( /^JavaScriptTest: Begin Run/)
            {
                if ($state eq 'starttest')
                {
                    $state = 'starttest';
                }
                elsif ($state eq 'initialized' && $mode eq 'nonrestart')
                {
                    $state = 'starttest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: starttest or initialized in non restart mode, mode $mode, log: $file";
                    $state = 'starttest';
                }
            }
            elsif ( ($tmp_test_id) = $_ =~ /^Spider: Begin loading http.*test=([^;]*);/)
            {
                if ($mode eq 'restart' && $test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected starttest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'starttest')
                {
                    $state = 'loadingtest';
                }
                elsif ($state eq 'initialized' && $mode eq 'nonrestart')
                {
                    $state = 'loadingtest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: starttest or initialized in non restart mode, log: $file";
                    $state = 'loadingtest';
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id) = $_ =~ /^JavaScriptTest: Begin Test ([^ ]*)/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected loadingtest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'loadingtest')
                {
                    $state = 'runningtest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: loadingtest, log: $file";
                    $state = 'runningtest';
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id) = $_ =~ /^jstest: (.*?) *bug:/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected runningtest, reportingtest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'runningtest')
                {
                    $state = 'reportingtest';
                }
                elsif ($state eq 'reportingtest')
                {
                    $state = 'reportingtest';
                }
                elsif ($state eq 'pendingtest')
                {
                    $state = 'reportingtest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: runningtest, reportingtest, pendingtest, log: $file";
                    $state = 'reportingtest';
                }

                ($test_result)      = $_ =~ /result: (.*?) *type:/;
                ($tmp_test_type)    = $_ =~ /type: (.*?) *description:/;

                die "FATAL ERROR: jstest test type mismatch: start test_type: $test_type, current test_type: $tmp_test_type, test state: $state, log: $file" 
                    if ($test_type ne $tmp_test_type);

                ($test_description) = $_ =~ /description: (.*)/;

                if (!$test_description)
                {
                    $test_description = "";
                }
                $test_description .= ' ' . join '; ', @messages;

                outputrecord $tmp_test_id, $test_description, $test_result;

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id) = $_ =~ /^Spider: Finish loading http.*test=([^;]*);/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected reportingtest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'reportingtest')
                {
                    $state = 'loadedtest';
                }
                else
                {
                    # probably an out of memory error or a browser only delayed execution test.
                    dbg "state: $state, expected: reportingtest. assuming test result is pending";
                    $state = 'pendingtest';
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id) = $_ =~ /^Spider: Current Url:.*test=([^;]*);/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected loadedtest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'loadedtest')
                {
                    $state = 'loadedtest';
                }
                elsif ($state eq 'reportingtest')
                {
                    $state = 'loadedtest';
                }
                elsif ($state eq 'pendingtest')
                {
                    $state = 'pendingtest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: loadedtest, reportingtest, pendingtest, log: $file";
                    $state = 'loadedtest';
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id, $page_status) = $_ =~ /^http:.*test=([^;]*);.* (PAGE STATUS: NORMAL.*)/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected loadedtest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'loadedtest')
                {
                    $state = 'loadedtest';
                }
                elsif ($state eq 'pendingtest')
                {
                    $state = 'pendingtest';
                }
                elsif ($state eq 'reportingtest')
                {
                    # test was pending, but output a result.
                    $state = 'loadedtest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: loadedtest, pendingtest, reportingtest, log: $file";
                    $state = 'loadedtest';
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id, $page_status) = $_ =~ /^http:.*test=([^;]*);.* (PAGE STATUS: TIMED OUT.*)/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected loadedtest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'loadedtest')
                {
                    $state = 'endrun';
                }
                elsif ($state eq 'runningtest')
                {
                    $state = 'completedtest';
                }
                elsif ($state eq 'reportingtest')
                {
                    $state = 'completedtest';
                }
                elsif ($state eq 'pendingtest')
                {
                    $state = 'completedtest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: loadedtest, runningtest, reportingtest, pendingtest, log: $file";
                    $state = 'endrun';
                }

                $test_result     = 'FAILED';
                $test_description = $page_status . ' ' . join '; ', @messages;;
                
                outputrecord $tmp_test_id, $test_description, $test_result;

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id) = $_ =~ /^JavaScriptTest: ([^ ]*) Elapsed time/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected loadedtest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'loadedtest')
                {
                    $state = 'completedtest';
                }
                elsif ($state eq 'pendingtest')
                {
                    $state = 'pendingtest';
                }
                elsif ($state eq 'reportingtest')
                {
                    # test was pending, but has been reported.
                    $state = 'completedtest';
                }
                else
                {
                    warn "WARNING: state: $state, expected: loadedtest, loadedtest, pendingtest, reportingtest, log: $file";
                    $state = 'completedtest';
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id) = $_ =~ /^JavaScriptTest: End Test ([^ ]*)/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected completedtest, endrun. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'completedtest')
                {
                    if ($mode eq 'restart')
                    {
                        $state = 'completedtest';
                    }
                    else
                    {
                        $state = 'starttest';
                    }
                }
                elsif ($state eq 'pendingtest')
                {
                    $state = 'completedtest';

                    $test_result = 'UNKNOWN';
                    $test_description = 'No test results reported. ' . join '; ', @messages;
                    
                    outputrecord $tmp_test_id, $test_description, $test_result;
                }
                elsif ($state eq 'endrun')
                {
                    $state = 'endrun';
                }
                else
                {
                    warn "WARNING: state: $state, expected: completedtest, pendingtest, endrun, log: $file";
                    $state = 'completedtest';
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( /^JavaScriptTest: End Run/)
            {
                if ($state eq 'completedtest')
                {
                    $state = 'endrun';
                }
                elsif ($state eq 'loadedtest')
                {
                    $state = 'endrun';
                }
                elsif ($state eq 'pendingtest')
                {
                    $state = 'pendingtest';
                }
                elsif ($state eq 'starttest' && $mode eq 'nonrestart')
                {
                    # non restart mode, at last test.
                    $state = 'endrun';
                }
                else
                {
                    warn "WARNING: state: $state, expected: completedtest, loadedtest, pendingtest or starttest in non restart mode, log: $file";
                    $state = 'endrun';
                }
            }
            elsif ( ($tmp_test_id, $exit_status) = $_ =~ /^http:.*test=([^;]*);.* (EXIT STATUS: NORMAL.*)/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected endrun. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'endrun')
                {
                    $state = 'endrun';
                }
                elsif ($state eq 'completedtest')
                {
                    dbg "previously pending test $test_id{$state} completed and is now endrun";
                    $state = 'endrun';
                }
                else
                {
                    warn "WARNING: state: $state, expected: endrun, log: $file";
                    $state = 'endrun';
                }

                if (! $test_reported{$tmp_test_id})
                {
                    $test_result = 'UNKNOWN';
                    $test_description = $exit_status . ' No test results reported. ' . join '; ', @messages;
                    
                    outputrecord $tmp_test_id, $test_description, $test_result;
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id, $exit_status) = $_ =~ /^http:.*test=([^;]*);.* (EXIT STATUS: TIMED OUT.*)/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected endrun. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'endrun')
                {
                    $state = 'endrun';
                }
                else
                {
                    dbg "state: $state, expected: endrun";
                    $state = 'endrun';
                }

                $test_result     = 'FAILED';
                $test_description = $exit_status . ' ' . join '; ', @messages;
                
                outputrecord $tmp_test_id, $test_description, $test_result;

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( ($tmp_test_id, $exit_status) = $_ =~ /^http:.*test=([^;]*);.* (EXIT STATUS: CRASHED.*)/)
            {
                if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
                {
                    warn "WARNING: state: $state, expected endrun. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
                }

                if ($state eq 'endrun')
                {
                    $state = 'endrun';
                }
                else
                {
                    dbg "state: $state, expected: endrun";
                    $state = 'endrun';
                }

                $test_result     = 'FAILED';
                $test_description = $exit_status . ' ' . join '; ', @messages;;
                
                outputrecord $tmp_test_id, $test_description, $test_result;

                $test_id{$state} = $tmp_test_id;
            }
            elsif ( /^(\/cygdrive\/.)?\/work\/mozilla\/mozilla\.com\/test\.mozilla\.com\/www$/)
            {
                if ($state eq 'endrun')
                {
                    $state = 'success';
                }
                else
                {
                    warn "WARNING: state: $state, expected: endrun, log: $file";
                    $state = 'success';
                }

                $test_id{$state} = $tmp_test_id;
            }
            elsif (!/^ \=\>/ && !/^\s+$/ && !/^[*][*][*]/ && !/^[-+]{2,2}(WEBSHELL|DOMWINDOW)/ && !/^Spider:/ && 
                   !/^JavaScriptTest:/ && !/real.*user.*sys.*$/ && !/user.*system.*elapsed/)
            {
                if ('starttest, loadingtest, runningtest, reportingtest, pendingtest, loadedtest, endrun, completedtest' =~ /$state/)
                {

                    if (/error: can.t allocate region/ || /set a breakpoint in malloc_error_break/ || 
                        /set a breakpoint in szone_error to debug/ || /malloc:.*mmap/ || /vm_allocate/ )
                    {
                        dbg "Adding message: $_ converted to /$test_id{$state}:0: out of memory";
                        push @messages, ('/' . $test_id{$state} . ':0: out of memory');
                    }
                    else
                    {
                        dbg "Adding message: $_";
                        push @messages, ($_);
                    }
                }
            }

            if ($debug)
            {
                if ($test_id{$state})
                {
                    dbg "test_id{$state}=$test_id{$state}, " . join '; ', @messages;
                }
                else
                {
                    dbg "state=$state, " . join '; ', @messages;
                }
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

    die "FATAL ERROR: Test run terminated prematurely. state: $state, log: $file" if ($state ne 'success');
}

close TEMP;

outresults;

unlink $temp;

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
    my ($test_id, $test_description, $test_result) = @_;

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

    $test_reported{$test_id} = 1;
    @messages = ();
}
