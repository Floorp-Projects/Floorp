#!/usr/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

package Moz::Milestone;
use strict;

use vars qw($officialMilestone
            $milestone);

local $Moz::Milestone::milestone;
local $Moz::Milestone::officialMilestone;

#
# Usage:  getOfficialMilestone($milestoneFile)
# Returns full milestone (x.x.x.x[ab12pre+])
#
sub getOfficialMilestone($) {
  my $mfile = $_[0];
  open(FILE,"$mfile") ||
    die ("Can't open $mfile for reading!");

  my $num = <FILE>;
  while($num =~ /^\s*#/ || $num !~ /^\d/) {
      $num = <FILE>;
  }

  close(FILE);
  if ($num !~ /^\d/) { return; }
  chomp($num);
  # Remove extra ^M caused by using dos-mode line-endings
  chop $num if (substr($num, -1, 1) eq "\r");
  $Moz::Milestone::officialMilestone = $num;
  $Moz::Milestone::milestone = &getMilestoneNum;
  return $num;
}

#
# Usage: getMilestoneNum($num)
# Returns: milestone without a + if it exists.
#
sub getMilestoneNum {
  if (defined($Moz::Milestone::milestone)) {
    return $Moz::Milestone::milestone;
  }

  if (defined($Moz::Milestone::officialMilestone)) {
    $Moz::Milestone::milestone = $Moz::Milestone::officialMilestone;
  } else {
    $Moz::Milestone::milestone = $_[0];
  }

  if ($Moz::Milestone::milestone =~ /\+$/) {    # for x.x.x+, strip off the +
    $Moz::Milestone::milestone =~ s/\+$//;
  }

  return $Moz::Milestone::milestone;
}

#
# Usage: getMilestoneQualifier($num)
# Returns: + if it exists.
#
sub getMilestoneQualifier {
  my $milestoneQualifier;
  if (defined($Moz::Milestone::officialMilestone)) {
    $milestoneQualifier = $Moz::Milestone::officialMilestone;
  } else {
    $milestoneQualifier = $_[0];
  }

  if ($milestoneQualifier =~ /\+$/) {
    return "+";
  }
}

sub getMilestoneMajor {
  my $milestoneMajor;
  if (defined($Moz::Milestone::milestone)) {
    $milestoneMajor = $Moz::Milestone::milestone;
  } else {
    $milestoneMajor = $_[0];
  }
  my @parts = split(/\./,$milestoneMajor);
  return $parts[0];
}

sub getMilestoneMinor {
  my $milestoneMinor;
  if (defined($Moz::Milestone::milestone)) {
    $milestoneMinor = $Moz::Milestone::milestone;
  } else {
    $milestoneMinor = $_[0];
  }
  my @parts = split(/\./,$milestoneMinor);

  if ($#parts < 1 ) { return 0; }
  return $parts[1];
}

sub getMilestoneMini {
  my $milestoneMini;
  if (defined($Moz::Milestone::milestone)) {
    $milestoneMini = $Moz::Milestone::milestone;
  } else {
    $milestoneMini = $_[0];
  }
  my @parts = split(/\./,$milestoneMini);

  if ($#parts < 2 ) { return 0; }
  return $parts[2];
}

sub getMilestoneMicro {
  my $milestoneMicro;
  if (defined($Moz::Milestone::milestone)) {
    $milestoneMicro = $Moz::Milestone::milestone;
  } else {
    $milestoneMicro = $_[0];
  }
  my @parts = split(/\./,$milestoneMicro);

  if ($#parts < 3 ) { return 0; }
  return $parts[3];
}

sub getMilestoneAB {
  my $milestoneAB;
  if (defined($Moz::Milestone::milestone)) {
    $milestoneAB = $Moz::Milestone::milestone;
  } else {
    $milestoneAB = $_[0];
  }
  
  if ($milestoneAB =~ /a/) { return "alpha"; }
  if ($milestoneAB =~ /b/) { return "beta"; }
  return "final";
}

#
# Usage:  getMilestoneABWithNum($milestoneFile)
# Returns the alpha and beta tag with its number (a1, a2, b3, ...)
#
sub getMilestoneABWithNum {
  my $milestoneABNum;
  if (defined($Moz::Milestone::milestone)) {
    $milestoneABNum = $Moz::Milestone::milestone;
  } else {
    $milestoneABNum = $_[0];
  }

  if ($milestoneABNum =~ /([ab]\d+)/) {
    return $1;
  } else {
    return "";
  }
}

#
# build_file($template_file,$output_file)
#
sub build_file($$) {
  my @FILE;
  my @MILESTONE_PARTS;
  my $MINI_VERSION = 0;
  my $MICRO_VERSION = 0;
  my $OFFICIAL = 0;
  my $QUALIFIER = "";

  if (!defined($Moz::Milestone::milestone)) { die("$0: no milestone file set!\n"); }
  @MILESTONE_PARTS = split(/\./, &getMilestoneNum);
  if ($#MILESTONE_PARTS >= 2) {
    $MINI_VERSION = 1;
  } else {
    $MILESTONE_PARTS[2] = 0;
  }
  if ($#MILESTONE_PARTS >= 3) {
    $MICRO_VERSION = 1;
  } else {
    $MILESTONE_PARTS[3] = 0;
  }
  if (! &getMilestoneQualifier) {
    $OFFICIAL = 1;
  } else {
    $QUALIFIER = "+";
  }

  if (-e $_[0]) {
    open(FILE, "$_[0]") || die("$0: Can't open $_[0] for reading!\n");
    @FILE = <FILE>;
    close(FILE);

    open(FILE, ">$_[1]") || die("$0: Can't open $_[1] for writing!\n");

    #
    # There will be more of these based on what we need for files.
    #
    foreach(@FILE) {
      s/__MOZ_MAJOR_VERSION__/$MILESTONE_PARTS[0]/g;
      s/__MOZ_MINOR_VERSION__/$MILESTONE_PARTS[1]/g;
      s/__MOZ_MINI_VERSION__/$MILESTONE_PARTS[2]/g;
      s/__MOZ_MICRO_VERSION__/$MILESTONE_PARTS[3]/g;
      if ($MINI_VERSION) {
        s/__MOZ_OPTIONAL_MINI_VERSION__/.$MILESTONE_PARTS[2]/g;
      }
      if ($MICRO_VERSION) {
        s/__MOZ_OPTIONAL_MICRO_VERSION__/.$MILESTONE_PARTS[3]/g;
      }

      print FILE $_;
    }
    close(FILE);
  } else {
    die("$0: $_[0] doesn't exist for autoversioning!\n");
  }

}

1;
