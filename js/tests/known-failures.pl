#!/usr/bin/perl
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

use strict;
use Getopt::Mixed "nextOption";

# predeclarations
sub debug;
sub usage;
sub parse_options;
sub escape_pattern;
sub unescape_pattern;

# option arguments

my $option_desc = "b=s branch>b T=s buildtype>T t=s testtype>t l=s rawlogfile>l f=s failurelogfile>f o=s os>o r=s patterns>r z=s timezone>z O=s outputprefix>O A=s arch>A K=s kernel>K D debug>D";

my $branch;
my $buildtype;
my $testtype;
my $rawlogfile;
my $failurelogfile;
my $os;
my $patterns;
my $timezone;
my $outputprefix;
my $arch;
my $kernel;
my $debug = $ENV{DEBUG};

# pattern variables

my $knownfailurebranchpattern;
my $failurebranchpattern;
my $knownfailureospattern;
my $failureospattern;
my $knownfailurebuildtypepattern;
my $failurebuildtypepattern;
my $knownfailuretesttypepattern;
my $failuretesttypepattern;
my $knownfailuretimezonepattern;
my $failuretimezonepattern;
my $knownfailurearchpattern;
my $failurearchpattern;
my $knownfailurekernelpattern;
my $failurekernelpattern;

my @patterns;
my $pattern;
my @failures;
my @fixes;
my @excludedtests;
my $excludedtest;
my @results;

my $regchars = '\[\^\-\]\|\{\}\?\*\+\.\<\>\$\(\)';

my @excludedfiles = ('spidermonkey-n.tests', 'slow-n.tests', 'performance.tests');
my $excludedfile;

&parse_options;

my $jsdir = $ENV{TEST_JSDIR};

if (!defined($jsdir)) {
     $jsdir = "/work/mozilla/mozilla.com/test.mozilla.com/www/tests/mozilla.org/js";
}

 # create working patterns file consisting of matches to users selection
 # and which has the test description patterns escaped

 # remove the excluded tests from the possible fixes log


foreach $excludedfile ( @excludedfiles ) {
    open EXCLUDED, "<$jsdir/$excludedfile" or die "Unable to open excluded file $jsdir/$excludedfile: $!\n";
    while (<EXCLUDED>) {
        chomp;

        next if ($_ =~ /^\#/);

        push @excludedtests, ($_);
    }
    close EXCLUDED;
}

@excludedtests = sort @excludedtests;

debug "loading patterns $patterns";
debug "pattern filter: /^TEST_ID=[^,]*, TEST_BRANCH=$knownfailurebranchpattern, TEST_RESULT=[^,]*, TEST_BUILDTYPE=$knownfailurebuildtypepattern, TEST_TYPE=$knownfailuretesttypepattern, TEST_OS=$knownfailureospattern, TEST_MACHINE=[^,]*, TEST_PROCESSORTYPE=$knownfailurearchpattern, TEST_KERNEL=$knownfailurekernelpattern, TEST_DATE=[^,]*, TEST_TIMEZONE=$knownfailuretimezonepattern,/\n";

open PATTERNS, "<$patterns" or die "Unable to open known failure patterns file $patterns: $!\n";
while (<PATTERNS>) {
    chomp;

    if ($_ =~ /^TEST_ID=[^,]*, TEST_BRANCH=$knownfailurebranchpattern, TEST_RESULT=[^,]*, TEST_BUILDTYPE=$knownfailurebuildtypepattern, TEST_TYPE=$knownfailuretesttypepattern, TEST_OS=$knownfailureospattern, TEST_MACHINE=[^,]*, TEST_PROCESSORTYPE=$knownfailurearchpattern, TEST_KERNEL=$knownfailurekernelpattern, TEST_DATE=[^,]*, TEST_TIMEZONE=$knownfailuretimezonepattern,/) {
        debug "adding pattern: $_";
        push @patterns, (escape_pattern($_));   
    }
    else {
        debug "skipping pattern: $_";
    }

}
close PATTERNS;

 # create a working copy of the current failures which match the users selection

debug "failure filter: ^TEST_ID=[^,]*, TEST_BRANCH=$failurebranchpattern, TEST_RESULT=FAIL[^,]*, TEST_BUILDTYPE=$failurebuildtypepattern, TEST_TYPE=$failuretesttypepattern, TEST_OS=$failureospattern, TEST_MACHINE=[^,]*, TEST_PROCESSORTYPE=$failurearchpattern, TEST_KERNEL=$failurekernelpattern, TEST_DATE=[^,]*, TEST_TIMEZONE=$failuretimezonepattern,";

if (defined($rawlogfile)) {

    $failurelogfile = "$outputprefix-results-failures.log";
    my $alllog      = "$outputprefix-results-all.log";

    debug "writing failures $failurelogfile";

    open INPUTLOG, "$jsdir/post-process-logs.pl $rawlogfile|sort|uniq|" or die "Unable to open $rawlogfile $!\n";
    open ALLLOG, ">$alllog" or die "Unable to open $alllog $!\n";
    open FAILURELOG, ">$failurelogfile" or die "Unable to open $failurelogfile $!\n";

    while (<INPUTLOG>) {
        chomp;

        print ALLLOG "$_\n";

        if ($_ =~ /^TEST_ID=[^,]*, TEST_BRANCH=$failurebranchpattern, TEST_RESULT=FAIL[^,]*, TEST_BUILDTYPE=$failurebuildtypepattern, TEST_TYPE=$failuretesttypepattern, TEST_OS=$failureospattern, TEST_MACHINE=[^,]*, TEST_PROCESSORTYPE=$failurearchpattern, TEST_KERNEL=$failurekernelpattern, TEST_DATE=[^,]*, TEST_TIMEZONE=$failuretimezonepattern,/) {
            debug "failure: $_";
            push @failures, ($_);
            print FAILURELOG "$_\n";
        }
    }
    close INPUTLOG;
    close ALLLOG;
    close FAILURELOG;

}
else {

    debug "loading failures $failurelogfile";

    open FAILURES, "<$failurelogfile" or die "Unable to open current failure log $failurelogfile: $!\n";
    while (<FAILURES>) {
        chomp;

        if ($_ =~ /^TEST_ID=[^,]*, TEST_BRANCH=$failurebranchpattern, TEST_RESULT=FAIL[^,]*, TEST_BUILDTYPE=$failurebuildtypepattern, TEST_TYPE=$failuretesttypepattern, TEST_OS=$failureospattern, TEST_MACHINE=[^,]*, TEST_PROCESSORTYPE=$failurearchpattern, TEST_KERNEL=$failurekernelpattern, TEST_DATE=[^,]*, TEST_TIMEZONE=$failuretimezonepattern,/) {
            debug "failure: $_";
            push @failures, ($_);
        }
    }
    close FAILURES;
}

debug "finding fixed bugs";

unlink "$outputprefix-results-possible-fixes.log";

foreach $pattern (@patterns) {
    # look for known failure patterns that don't have matches in the 
    # the current failures selected by the user.

    @results = grep m@^$pattern@, @failures;

    if ($#results == -1) {
        debug "fix: $pattern";
        push @fixes, ($pattern)
    }
}

foreach $excludedtest ( @excludedtests ) {

    if ($debug) {
        @results = grep m@$excludedtest@, @fixes;
        if ($#results > -1) {
            print "excluding: " . (join ', ', @results) . "\n";
        }
    }

    @results = grep !m@$excludedtest@, @fixes;

    @fixes = @results;
}

my $fix;
open OUTPUT, ">$outputprefix-results-possible-fixes.log" or die "Unable to open $outputprefix-results-possible-fixes.log: $!";
foreach $fix (@fixes) {
    print OUTPUT unescape_pattern($fix) . "\n";
    if ($debug) {
        debug "fix: $fix";
    }
}
close OUTPUT;

print STDOUT "$outputprefix-results-possible-fixes.log\n";

debug "finding regressions";

my $pass = 0;
my $changed = ($#patterns != -1);

while ($changed) {

    $pass = $pass + 1;

    $changed = 0;

    debug "pass $pass";

    foreach $pattern (@patterns) {

        debug "Pattern: $pattern";

        my @nomatches = grep !m@^$pattern@, @failures;
        my @matches   = grep m@^$pattern@, @failures;

        if ($debug) {
            my $temp = join ', ', @nomatches;
            debug "nomatches: $#nomatches $temp";
            $temp = join ', ', @matches;
            debug "matches: $#matches $temp";
        }

        @failures = @nomatches;

        if ($#matches > -1) {
            $changed = 1;
        }

        debug "*****************************************";
    }

}

foreach $excludedtest ( @excludedtests ) {

    if ($debug) {
        @results = grep m@$excludedtest@, @failures;
        if ($#results > -1) {
            print "excluding: " . (join ', ', @results) . "\n";
        }
    }

    @results = grep !m@$excludedtest@, @failures;

    @failures = @results;
}

open OUTPUT, ">$outputprefix-results-possible-regressions.log" or die "Unable to open $outputprefix-results-possible-regressions.log: $!";

my $failure;
foreach $failure (@failures) {
    print OUTPUT "$failure\n";
    if ($debug) {
        debug "regression: $failure";
    }
}
close OUTPUT;

print STDOUT "$outputprefix-results-possible-regressions.log\n";


sub debug {
    if ($debug) {
        my $msg = shift;
        print "DEBUG: $msg\n";
    }
}

sub usage {

    my $msg = shift;

    print STDERR <<EOF;

usage: $msg

known-failures.pl [-b|--branch] branch [-T|--buildtype] buildtype 
                  [-t|--testtype] testtype [-o os|--os] 
                  ([-f|--failurelogfile] failurelogfile|[-l|--logfile] rawlogfile])
                  [-r|--patterns] patterns [-z|--timezone] timezone 
                  [-O|--outputprefix] outputprefix

    variable            description
    ===============     ============================================================
    -b branch           branch 1.8.1, 1.9.0, all
    -T buildtype        build type opt, debug, all
    -t testtype         test type browser, shell, all
    -l rawlogfile       raw logfile
    -f failurelogfile   failure logfile
    -o os               operating system win32, mac, linux, all
    -r patterns         known failure patterns
    -z timezone         -0400, -0700, etc. default to user\'s zone
    -O outputprefix     output files will be generated with this prefix
    -A arch             architecture, all or a specific pattern
    -K kernel           kernel, all or a specific pattern
    -D                  turn on debugging output
EOF

    exit(2);
}

sub parse_options {
    my ($option, $value);

    Getopt::Mixed::init ($option_desc);
    $Getopt::Mixed::order = $Getopt::Mixed::RETURN_IN_ORDER;

    while (($option, $value) = nextOption()) {

        if ($option eq "b") {
            $branch = $value;
        }
        elsif ($option eq "T") {
            $buildtype = $value;
        }
        elsif ($option eq "t") {
            $testtype = $value;
        }
        elsif ($option eq "l") {
            $rawlogfile = $value;
        }
        elsif ($option eq "f") {
            $failurelogfile = $value;
        }
        elsif ($option eq "o") {
            $os = $value;
        }
        elsif ($option eq "r") {
            $patterns = $value;
        }
        elsif ($option eq "z") {
            $timezone = $value;
        }
        elsif ($option eq "O") {
            $outputprefix = $value;
        }
        elsif ($option eq "A") {
            $arch = $value;
        }
        elsif ($option eq "K") {
            $kernel = $value;
        }
        elsif ($option eq "D") {
            $debug = 1;
        }

    }

    if ($debug) {
        print "branch=$branch, rawlogfile=$rawlogfile failurelogfile=$failurelogfile, os=$os, buildtype=$buildtype, testtype=$testtype, patterns=$patterns, timezone=$timezone, outputprefix=$outputprefix\n";
    }
    Getopt::Mixed::cleanup();

    if ( !defined($branch) ) {
        usage "missing branch";
    }

    if (!defined($rawlogfile) && !defined($failurelogfile)) {
        usage "missing logfile";
    }


    if (!defined($os)) { 
        usage "missing os";
    }

    if (!defined($buildtype)) {
        usage "missing buildtype";
    }

    if (!defined($testtype)) {
        usage "missing testtype";
    }

    if (!defined($patterns)) {
        usage "missing patterns";
    }


    if (!defined($timezone)) {
        usage "missing timezone";
    }


    if (!defined($outputprefix)) {
        usage "missing outputprefix";
    }

    if ($branch eq "1.8.1") {
        $knownfailurebranchpattern = "([^,]*1\\.8\\.1[^,]*|\\.\\*)";
        $failurebranchpattern      = "1\\.8\\.1";
    }
    elsif ($branch eq "1.9.0") {
        $knownfailurebranchpattern = "([^,]*1\\.9\\.0[^,]*|\\.\\*)";
        $failurebranchpattern      = "1\\.9\\.0";
    }
    elsif ($branch eq "all") {
        $knownfailurebranchpattern = "[^,]*";
        $failurebranchpattern      = "[^,]*";
    }

    if ($os eq "win32") {
        $knownfailureospattern     = "([^,]*win32[^,]*|\\.\\*)";
        $failureospattern          = "win32";
    }
    elsif ($os eq "mac") {
        $knownfailureospattern     = "([^,]*mac[^,]*|\\.\\*)";
        $failureospattern          = "mac";
    }
    elsif ($os eq "linux") {
        $knownfailureospattern     = "([^,]*linux[^,]*|\\.\\*)";
        $failureospattern          = "linux";
    }
    elsif ($os eq "all") {
        $knownfailureospattern     = "[^,]*";
        $failureospattern          = "[^,]*";
    }

    if ($buildtype eq "opt") {
        $knownfailurebuildtypepattern = "([^,]*opt[^,]*|\\.\\*)";
        $failurebuildtypepattern      = "opt";
    }
    elsif ($buildtype eq "debug") {
        $knownfailurebuildtypepattern = "([^,]*debug[^,]*|\\.\\*)";
        $failurebuildtypepattern      = "debug";
    }
    elsif ($buildtype eq "all") {
        $knownfailurebuildtypepattern = "[^,]*";
        $failurebuildtypepattern      = "[^,]*";
    }

    if ($testtype eq "shell") {
        $knownfailuretesttypepattern = "([^,]*shell[^,]*|\\.\\*)";
        $failuretesttypepattern      = "shell";
    }
    elsif ($testtype eq "browser") {
        $knownfailuretesttypepattern = "([^,]*browser[^,]*|\\.\\*)";
        $failuretesttypepattern      = "browser";
    }
    elsif ($testtype eq "all") {
        $knownfailuretesttypepattern = "[^,]*";
        $failuretesttypepattern      = "[^,]*";
    }

    if ($timezone eq "all") {
        $knownfailuretimezonepattern = "[^,]*";
        $failuretimezonepattern      = "[^,]*";
    }
    else {
        $knownfailuretimezonepattern = "([^,]*" . $timezone . "[^,]*|\\.\\*)";
        $failuretimezonepattern      = "$timezone";
    }

    if ($arch ne "all") {
        $knownfailurearchpattern = "([^,]*" . $arch . "[^,]*|\\.\\*)";
        $failurearchpattern      = "$arch";
    }
    else {
        $knownfailurearchpattern = "[^,]*";
        $failurearchpattern      = "[^,]*";
    }

    if ($kernel ne  "all") {
        $knownfailurekernelpattern = "([^,]*" . $kernel . "[^,]*|\\.\\*)";
        $failurekernelpattern      = "$kernel";
    }
    else {
        $knownfailurekernelpattern = "[^,]*";
        $failurekernelpattern      = "[^,]*";
    }


}

sub escape_pattern {

    my $line = shift;

    chomp;

    my ($leading, $trailing) = $line =~ /(.*TEST_DESCRIPTION=)(.*)/;

    #    debug "escape_pattern: before: $leading$trailing";

    # replace unescaped regular expression characters in the 
    # description so they are not interpreted as regexp chars
    # when matching descriptions. leave the escaped regexp chars
    # `regexp alone so they can be unescaped later and used in 
    # pattern matching.

    # see perldoc perlre

    $trailing =~ s/\\/\\\\/g;

    # escape non word chars that aren't surrounded by ``
    $trailing =~ s/(?<!`)([$regchars])(?!`)/\\$1/g;
    $trailing =~ s/(?<!`)([$regchars])(?=`)/\\$1/g;
    $trailing =~ s/(?<=`)([$regchars])(?!`)/\\$1/g;
    #    $trailing =~ s/(?<!`)(\W)(?!`)/\\$1/g;

    # unquote the regchars
    $trailing =~ s/\`([^\`])\`/$1/g;

    debug "escape_pattern: after: $leading$trailing";

    return "$leading$trailing";

}

sub unescape_pattern {
    my $line = shift;

    chomp;

    my ($leading, $trailing) = $line =~ /(.*TEST_DESCRIPTION=)(.*)/;

    # quote the unescaped non word chars
    $trailing =~ s/(?<!\\)([$regchars])/`$1`/g;

    # unescape the escaped non word chars
    $trailing =~ s/\\([$regchars])/$1/g;

    $trailing =~ s/\\\\/\\/g;

    debug "unescape_pattern: after: $leading$trailing";

    return "$leading$trailing";
}

####


1;
