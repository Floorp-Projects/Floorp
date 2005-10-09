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
# The Original Code is JavaScript Engine testing utilities.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary
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

use URI::Escape;
use Time::HiRes qw(sleep);
use lib "$ENV{HOME}/projects/tests/www/bin/";

require "edit-talkback.pl";

my $bindir     = shift @ARGV || die usage();
my $timeout    = shift @ARGV || die usage();
my $browserexe = shift @ARGV || die usage();
my $profile    = shift @ARGV || die usage();
my $jsfile     = shift @ARGV || die usage();
my $hook       = shift @ARGV || die usage();

my $chromeurl;
my $testUrl;

editTalkback($browserexe, $jsfile);

my $testrunner = "http://" . 
    $ENV{TEST_HTTP} . 
    "/tests/mozilla.org/js/js-test-driver-quirks.html?" .
    "test=$jsfile;language=language;javascript";

my $spider = "chrome://spider/content/open.xul?" .
    "depth=0&timeout=120&waittime=5&hooksignal=on&autostart=on&autoquit=on&javascripterrors=on&" .
    "javascriptwarnings=off&chromeerrors=on&xblerrors=on&csserrors=off&" .
    "scripturl=" . 
    uri_escape("http://" . 
               $ENV{TEST_HTTP} . 
               "/tests/mozilla.org/js/$hook") . 
    "&" .
    "url=" . uri_escape(uri_escape($testrunner));


my @args = ("$bindir/timed_run",
            $timeout,
            "$jsfile",
            $browserexe,
            "-P",
            $profile,
            "-chrome",
            $spider);
#print "args: " . join(',', @args) . "\n";
my $rc = system @args;
my $signal = $rc & 0x00ff;
if ($signal == 2)
{
    die "received signal: $signal\n";
}

sub usage
{
    return "Usage: each.pl bindir browserexe profile timeout jsfile hook\n";
}

1;
