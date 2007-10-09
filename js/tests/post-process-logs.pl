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
local $DEBUG = 0;

(undef, $temp) = tempfile();

open TEMP, ">$temp" or
    die "Unable to open temporary file $temp for writing: $!\n";

local ($test_id, $test_bug, $test_result, $test_type,
       $test_description, $in_browser_test, @messages,
       $test_processortype, $test_kernel, $test_suite,
       $last_test);

local $count_begintests = 0;
local $count_jstests = 0;
local $count_exitonlytestsnormal = 0;
local $count_exitonlytestsabnormal = 0;
local $count_records = 0;

while ($file = shift @ARGV)
{
    dbg "initializing in_browser_test, messages";

    $in_browser_test = 0;
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

    open FILE, "$file" or die "unable to open $file for reading: $!\n";

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

	    if ( $_ =~ /^jstest: /)
	    {
		++$count_jstests;


		($test_id)          = $_ =~ /^jstest: (.*?) *bug:/;
		($test_bug)         = $_ =~ /bug: (.*?) *result:/;
		($test_result)      = $_ =~ /result: (.*?) *type:/;
		($test_type)        = $_ =~ /type: (.*?) *description:/;
		($test_description) = $_ =~ /description: (.*)/;

		if (!$test_description)
		{
		    $test_description = "";
		}

		dbg "test_id:          $test_id";
		dbg "test_bug:         $test_bug";
		dbg "test_result:      $test_result";
		dbg "test_type:        $test_type";
		dbg "test_description: $test_description";
		
		outputrecord;

		dbg "-";
	    }
	    elsif (($envvar, $envval) = $_ =~ /^environment: (TEST_[A-Z0-9_]*)=(.*)/ )
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
    }
    elsif ($test_product eq "firefox")
    {
	$test_id          = "__UNDEFINED__";
	$test_bug         = "__UNDEFINED__";
	$test_result      = "__UNDEFINED__";
	$test_type        = "__UNDEFINED__";
	$test_description = "__UNDEFINED__";

	$last_test        = "__UNDEFINED__";

	$in_browser_test = 0;
	@messages = ();

	while (<FILE>)
	{
	    chomp;

	    # remove carriage returns, bels and other annoyances.
	    $_ =~ s/[\r]$//;
	    $_ =~ s/[\r]/CR/g;
	    $_ =~ s/[\x01-\x08]//g;
	    $_ =~ s/\s+$//;

	    dbg  "INPUT: $_";

	    if ( $_ =~ /^jstest: /)
	    {
		++$count_jstests;


		($test_id)          = $_ =~ /^jstest: (.*?) *bug:/;
		($test_bug)         = $_ =~ /bug: (.*?) *result:/;
		($test_result)      = $_ =~ /result: (.*?) *type:/;
		($test_type)        = $_ =~ /type: (.*?) *description:/;
		($test_description) = $_ =~ /description: (.*)/;

		if (!$test_description)
		{
		    $test_description = "";
		}

		dbg "test_id:          $test_id";
		dbg "test_bug:         $test_bug";
		dbg "test_result:      $test_result";
		dbg "test_type:        $test_type";
		dbg "test_description: $test_description";

		dbg "test_result:      $test_result";

		outputrecord;
	    }
	    elsif ( ($test_id, $exit_status) = $_ =~ 
		    /^http:.*test=([^;]*);.*: EXIT STATUS: (.*)/)
	    {
		dbg "Processing EXIT STATUS: test_id: $test_id, exit_status: $exit_status";

		$test_buildtype = "nightly" unless $test_buildtype;
		$test_type = "browser";

		if ($exit_status =~ /^NORMAL/)
		{
		    if ($last_test ne $test_id)
		    {
			++$count_exitonlytestsnormal;

			$test_description = join '; ', @messages;
			if ($test_description =~ /(out of memory|malloc:.*vm_allocate)/i)
			{
			    $test_result = "FAILED";
			}
			else
			{
			    $test_result = "FAILED";
			}
			# if we got here, the test terminated but it
			# didn't report a result either because it 
			# crashed or the harness messed up.
			outputrecord;
		    }
		}
		else
		{
		    ++$count_exitonlytestsabnormal;

		    dbg "Abnormal result";

		    dbg "creating test_description from messages array";

		    $test_description = join '; ', @messages;
		    # if we got here, the test terminated but it
		    # didn't report a result either because it is one
		    # of the old style -n.js negative tests, it
		    # crashed or the harness messed up.

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
			$test_description = "EXIT STATUS: $exit_status, $test_description";
		    }
		    outputrecord;
		}

		$in_browser_test = 0;
		@messages = ();

		dbg "not in browser test";

		dbg "--";

	    }
	    elsif ( ($test_id) =
		    $_ =~ /Begin loading .*http.*test=([^;]*).*/)
	    {
		dbg "Begin loading $test_id";
		$in_browser_test = 1;
		++$count_begintests;
	    }
	    elsif ( ($test_id) =
		    $_ =~ /Finish loading .*http.*test=([^;]*).*/)
	    {
		dbg "Finish loading $test_id";
		if ($test_id eq "__UNDEFINED__")
		{
		    $test_id = $test_id;
		}
		dbg "test_id: $test_id, test_id: $test_id";
		die if ($test_id ne $test_id);
	    }
	    elsif ($in_browser_test)
	    {
		if (! /^[-+]{2,2}(WEBSHELL|DOMWINDOW)/ && ! /^Spider:/ && ! /^JavaScriptTest:/ && !/^WARNING:/)
		{

		    dbg "Saving message $_";
		    push @messages, ($_);
		}
	    }
	    elsif (($envvar, $envval) = $_ =~ /^environment: (TEST_[A-Z0-9_]*)=(.*)/ )
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

if ($DEBUG)
{
    print "begintests    $count_begintests\n";
    print "jstests       $count_jstests\n";
    print "exit normal   $count_exitonlytestsnormal\n";
    print "exit abnormal $count_exitonlytestsabnormal\n";
    print "total records $count_records\n";
    print "jstests + exit normal + exit abnormal = " . ($count_jstests + $count_exitonlytestsnormal + $count_exitonlytestsabnormal) . "\n";
}

die "test counts do not match: total records $count_records; jstests + exit normal + exit abnormal = " . 
    ($count_jstests + $count_exitonlytestsnormal + $count_exitonlytestsabnormal) . "\n" 
    if ($count_records != ($count_jstests + $count_exitonlytestsnormal + $count_exitonlytestsabnormal));

sub dbg {
    if ($DEBUG)
    {
	my $msg = shift;
	print STDOUT "DEBUG: $msg\n";
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

    if ($DEBUG)
    {
	print "RECORD: $output";
    }
    print TEMP $output;

    $last_test = $test_id;

    $test_id = "__UNDEFINED__";
    $test_bug = "__UNDEFINED__";
    $test_result = "__UNDEFINED__";
    $test_type = "__UNDEFINED__";
    $test_description = "__UNDEFINED__";

    
}
