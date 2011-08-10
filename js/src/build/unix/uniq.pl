#!/usr/bin/env perl

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
# The Original Code is this file as it was released upon December 26, 2000.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Christopher Seawood <cls@seawood.org>
#   Joey Armstrong <joey@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

##----------------------------##
##---] CORE/CPAN INCLUDES [---##
##----------------------------##
use strict;
use warnings;
use Getopt::Long;

##-------------------##
##---]  EXPORTS  [---##
##-------------------##
our $VERSION = qw(1.1);

##-------------------##
##---]  GLOBALS  [---##
##-------------------##
my %argv;
my $modver = $Getopt::Long::VERSION || 0;
my $isOldGetopt = ($modver eq '2.25') ? 1 : 0;

###########################################################################
## Intent: Script init function
###########################################################################
sub init
{
    if ($isOldGetopt)
    {
	# mozilla.build/mingw perl in need of an upgrade
	# emulate Getopt::Long switch|short:init
	foreach (qw(debug regex sort))
	{
	    if (defined($argv{$_}))
	    {
		$argv{$_} ||= 1;
	    }
	}
    }
} # init

##----------------##
##---]  MAIN  [---##
##----------------##
my @args = ($isOldGetopt)
    ? qw(debug|d regex|r sort|s)
    : qw(debug|d:1 regex|r:1 sort|s:1)
    ;

unless(GetOptions(\%argv, @args))
{
    print "Usage: $0\n";
    print "  --sort   Sort list elements early\n";
    print "  --regex  Exclude subdirs by pattern\n";
}

init();
my $debug = $argv{debug} || 0;

my %seen;
my @out;
my @in = ($argv{sort}) ? sort @ARGV : @ARGV;

foreach my $d (@in)
{
    next if ($seen{$d}++);

    print "   arg is $d\n" if ($debug);

    if ($argv{regex})
    {
        my $found = 0;
        foreach my $dir (@out)
	{
	    my $dirM = quotemeta($dir);
            $found++, last if ($d eq $dir || $d =~ m!^${dirM}\/!);
        }
	print "Adding $d\n" if ($debug && !$found);
        push @out, $d if (!$found);
    } else {
	print "Adding: $d\n" if ($debug);
        push(@out, $d);
    }
}

print "@out\n"

# EOF
