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
# The Original Code is this file as it was released upon March 8, 1999.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

# mddepend.pl - Reads in dependencies generated my -MD flag. Prints list
#   of objects that need to be rebuilt. These can then be added to the
#   PHONY target. Using this script copes with the problem of header
#   files that have been removed from the build.
#    
# Usage:
#   mddepend.pl <output_file> <dependency_files...>
#
# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).

#$debug = 1;

$outfile = shift @ARGV;
my $silent = $ENV{MAKEFLAGS} =~ /^\w*s|\s-s/;

@alldeps=();
# Parse dependency files
while ($line = <STDIN>) {
  chomp $line;
  # Remove extra ^M caused by using dos-mode line-endings
  chop $line if (substr($line, -1, 1) eq "\r");
  ($obj,$rest) = split /\s*:\s+/, $line, 2;
  next if $obj eq '';

  if ($line =~ /\\$/) {
    chop $rest;
    $hasSlash = 1;
  } else {
    $hasSlash = 0;
  }
  $deps = [ $obj, split /\s+/, $rest ];

  while ($hasSlash and $line = <STDIN>) {
    chomp $line;
    if ($line =~ /\\$/) {
      chop $line;
    } else {
      $hasSlash = 0;
    }
    $line =~ s/^\s+//;
    push @{$deps}, split /\s+/, $line;
  }
  warn "add @{$deps}\n" if $debug;
  push @alldeps, $deps;
}

# Test dependencies
foreach $deps (@alldeps) {
  $obj = shift @{$deps};

  $mtime = (stat $obj)[9] or next;

  foreach $dep_file (@{$deps}) {
    if (not defined($dep_mtime = $modtimes{$dep_file})) {
      $dep_mtime = (stat $dep_file)[9];
      $modtimes{$dep_file} = $dep_mtime;
    }
    if ($dep_mtime ne '' and $dep_mtime > $mtime) {
      print "$obj($mtime) older than $dep_file($dep_mtime)\n" if $debug;
      push @objs, $obj;
      # Object will be marked for rebuild. No need to check other dependencies.
      last;
    }
  }
}

# Output objects to rebuild (if needed).
if (@objs) {
  $new_output = "@objs: FORCE\n";

  # Read in the current dependencies file.
  open(OLD, "<$outfile")
    and $old_output = <OLD>;
  close(OLD);

  # Only write out the dependencies if they are different.
  if ($new_output ne $old_output) {
    open(OUT, ">$outfile") and binmode(OUT) and print OUT "$new_output";
    print "Updating dependencies file, $outfile\n" unless $silent;
    if ($debug) {
      print "new: $new_output\n";
      print "was: $old_output\n" if $old_output ne '';
    }
  }
} elsif (-s $outfile) {
  # Remove the old dependencies because all objects are up to date.
  print "Removing old dependencies file, $outfile\n" unless $silent;

  if ($debug) {
    open(OLD, "<$outfile")
      and $old_output = <OLD>;
    close(OLD);
    print "was: $old_output\n";
  }

  unlink $outfile;
}
