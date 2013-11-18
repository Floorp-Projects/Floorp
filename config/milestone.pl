#!/usr/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use Getopt::Long;

use strict;
use vars qw(
            $OBJDIR
            $SRCDIR
            $TOPSRCDIR
            $SCRIPTDIR
            @TEMPLATE_FILE
            $MILESTONE_FILE
            $MILESTONE
            $MILESTONE_NUM
            @MILESTONE_PARTS
            $MINI_VERSION
            $MICRO_VERSION
            $opt_debug
            $opt_template
            $opt_uaversion
            $opt_symbolversion
            $opt_help
            );

$SCRIPTDIR = $0;
$SCRIPTDIR =~ s/[^\/]*$//;
push(@INC,$SCRIPTDIR);

require "Moz/Milestone.pm";

&GetOptions('topsrcdir=s' => \$TOPSRCDIR, 'srcdir=s' => \$SRCDIR, 'objdir=s' => \$OBJDIR, 'debug', 'help', 'template', 'uaversion', 'symbolversion');

if (defined($opt_help)) {
    &usage();
    exit;
}

if (defined($opt_template)) {
    @TEMPLATE_FILE = @ARGV;
    if ($opt_debug) {
        print("TEMPLATE_FILE = --@TEMPLATE_FILE--\n");
    }
}

if (!defined($SRCDIR)) { $SRCDIR = '.'; }
if (!defined($OBJDIR)) { $OBJDIR = '.'; }

$MILESTONE_FILE  = "$TOPSRCDIR/config/milestone.txt";
@MILESTONE_PARTS = (0, 0, 0, 0);

#
# Grab milestone (top line of $MILESTONE_FILE that starts with a digit)
#
my $milestone = Moz::Milestone::getOfficialMilestone($MILESTONE_FILE);

if (defined(@TEMPLATE_FILE)) {
  my $TFILE;

  foreach $TFILE (@TEMPLATE_FILE) {
    my $BUILT_FILE = "$OBJDIR/$TFILE";
    $TFILE = "$SRCDIR/$TFILE.tmpl";

    if (-e $TFILE) {

      Moz::Milestone::build_file($TFILE,$BUILT_FILE);

    } else {
      warn("$0:  No such file $TFILE!\n");
    }
  }
} elsif(defined($opt_uaversion)) {
  # Only expose the major milestone in the UA string, hide the patch level
  # (bugs 572659 and 870868).
  my $uaversion = Moz::Milestone::getMilestoneMajor($milestone) . ".0";
  print "$uaversion\n";
} elsif(defined($opt_symbolversion)) {
  # Only expose major milestone and alpha version. Used for symbol versioning
  # on Linux.
  my $symbolversion = Moz::Milestone::getMilestoneMajor($milestone) .
                      Moz::Milestone::getMilestoneABWithNum($milestone);
  print "$symbolversion\n";
} else {
  print "$milestone\n";
}

sub usage() {
  print <<END
`milestone.pl [--topsrcdir TOPSRCDIR] [--objdir OBJDIR] [--srcdir SRCDIR] --template [file list] --uaversion --symbolversion`  # will build file list from .tmpl files
END
    ;
}
