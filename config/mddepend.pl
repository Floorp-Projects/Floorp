#!/usr/bin/env perl

# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is this file as it was released upon March 8, 1999.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1999
# Netscape Communications Corporation. All Rights Reserved.

# mddepend.pl - Reads in dependencies generated my -MD flag. Prints list
#   of objects that need to be rebuilt. These can then be added to the
#   PHONY target. Using this script copes with the problem of header
#   files that have been removed from the build.
#    
# Usage:
#   mddepend.pl <dependency files>
#
# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).

@alldeps=();
# Parse dependency files
while ($line = <>) {
  chomp $line;
  ($obj,$rest) = split /:\s+/o, $line, 2;
  next if $obj eq '';

  if ($line =~ /\\$/o) {
    chop $rest;
    $hasSlash = 1;
  } else {
    $hasSlash = 0;
  }
  $deps = [ $obj, split /\s+/o, $rest ];

  while ($hasSlash and $line = <>) {
    chomp $line;
    if ($line =~ /\\$/o) {
      chop $line;
    } else {
      $hasSlash = 0;
    }
    $line =~ s/^\s+//o;
    push @{$deps}, split /\s+/o, $line;
  }
  push @alldeps, $deps;
}

# Test dependencies
foreach $deps (@alldeps) {
  $obj = shift @{$deps};

  ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
   $atime,$mtime,$ctime,$blksize,$blocks) = stat($obj)
      or next;

  foreach $dep_file (@{$deps}) {
    if (not defined($dep_mtime = $modtimes{$dep_file})) {
      ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
       $atime,$dep_mtime,$ctime,$blksize,$blocks) = stat($dep_file);
      $modtimes{$dep_file} = $dep_mtime;
    }
    if ($dep_mtime eq '' or $dep_mtime > $mtime) {
      print "$obj ";
      $haveObjects = 1;
      last;
    }
  }
}
print ": FORCE\n" if $haveObjects;
