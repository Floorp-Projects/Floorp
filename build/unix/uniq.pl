#!/usr/bin/env perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
