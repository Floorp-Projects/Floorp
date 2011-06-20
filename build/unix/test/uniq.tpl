#!/usr/bin/env perl
###########################################################################
## Intent: Unit test to verify uniq.pl
###########################################################################

##----------------------------##
##---] CORE/CPAN INCLUDES [---##
##----------------------------##
use strict;
use warnings;
use Cwd;
use Getopt::Long;  # GetOptions

use Test;
sub BEGIN { plan tests => 12 }

##-------------------##
##---]  EXPORTS  [---##
##-------------------##
our $VERSION = qw(1.0);

##------------------##
##---] INCLUDES [---##
##------------------##
use FindBin;

##-------------------##
##---]  GLOBALS  [---##
##-------------------##
my %argv;


###########################################################################
## Intent: Run the arch command for output
##
## Returns:
##    0   on success
##    $?  command shell exit status
###########################################################################
sub uniq_pl
{
    my $cmd = "perl $FindBin::RealBin/../uniq.pl @_";
    print "Running: $cmd\n" if ($argv{debug});
    my @tmp = `$cmd 2>&1`;
    my @output = map{ split(/\s+/o); } @tmp;
    wantarray ? @output : "@output";
} # uniq_pl

###########################################################################
## Intent:
##
## Returns:
##    0 on success
###########################################################################
sub check_uniq
{
    print STDERR "Running test: check_uniq\n" if ($argv{debug});

    # TODO: improve test, uniq.pl regexpr handling not quite right

    my @todo =
      (
       [ '', qw(a a/b a/b/c) ] => [  qw(a a/b a/b/c) ],
       [ '', qw(a/b a a/b/c) ] => [ qw(a/b a a/b/c) ],
       [ '', qw(a/b/c a/b a) ] => [ qw(a/b/c a/b a) ],

       [ '', qw(a a/b a/b/c a/b a) ] => [  qw(a a/b a/b/c) ], # dup removal

       [ '-s', qw(a a/b a/b/c) ] => [ qw(a a/b a/b/c)  ],
       [ '-s', qw(a/b a a/b/c) ] => [ qw(a a/b a/b/c) ],
       [ '-s', qw(a/b/c a/b a) ] => [ qw(a a/b a/b/c) ],

       [ '-r', qw(a a/b a/b/c) ] => [ qw(a) ],
       [ '-r', qw(a/b a a/b/c) ] => [ qw(a/b a) ],
       [ '-r', qw(a/b/c a/b a) ] => [ qw(a/b/c a/b a) ],

       [ '-r', qw(. .. a/b ../a aa/bb) ] => [ qw(. .. a/b aa/bb) ],
       [ '-r', qw(.. a/b ../a . aa/bb) ] => [ qw(.. a/b . aa/bb) ],
      );

    my $ct=1;
    while (@todo)
    {
        my ($a, $b) = splice(@todo, 0, 2);
	my @args = @{ $a };
	my @exp = @{ $b };

	my @out = uniq_pl(@args);
#	compareExp(\@out, \@exp, 'Failed on line ' . __LINE__ . ", dataset $ct");
	if (0 && 7 == $ct)
	  {
	    print STDERR "\n";
	    print STDERR map{ "args> $_\n" }@args;
	    print STDERR "\n";
	    print STDERR map{ "exp> $_\n" }@exp;
	    print STDERR "\n";
	    print STDERR map{ "out> $_\n" }@out;
	  }

	ok("@out", "@exp", 'Failed on line ' . __LINE__ . ", dataset $ct");
	$ct++;
    }

} # check_uniq

###########################################################################
## Intent: Smoke tests for the unittests module
###########################################################################
sub smoke
{
    print STDERR "Running test: smoke()\n" if ($argv{debug});
} # smoke()

###########################################################################
## Intent: Intitialize global test objects and consts
###########################################################################
sub init
{
    print "Running: init()\n" if ($argv{debug});
#    testplan(24, 0);
} # init()

##----------------##
##---]  MAIN  [---##
##----------------##
unless(GetOptions(\%argv,
		  qw(
		     debug|d
                     manual
		     test=s@
		     verbose
		     )))
{
    print "USAGE: $0\n";
    print "  --debug    Enable script debug mode\n";
    print "  --fail     Force a testing failure condition\n";
    print "  --manual   Also run disabled tests\n";
    print "  --smoke    Run smoke tests then exit\n";
    print "  --test     Run a list of tests by function name\n";
    print "  --verbose  Enable script verbose mode\n";
    exit 1;
}

init();
testbyname(@{ $argv{test} }) if ($argv{test});
smoke();

check_uniq();
ok(1, 0, 'Forced failure by command line arg --fail') if ($argv{fail});

# EOF
