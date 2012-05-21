#!/usr/bin/env perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# mddepend.pl - Reads in dependencies generated my -MD flag. Prints list
#   of objects that need to be rebuilt. These can then be added to the
#   PHONY target. Using this script copes with the problem of header
#   files that have been removed from the build.
#    
# Usage:
#   mddepend.pl <output_file> <dependency_files...>
#
# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).

use strict;

use constant DEBUG => 0;

my $outfile = shift @ARGV;
my $silent = $ENV{MAKEFLAGS} =~ /^\w*s|\s-s/;

my $line = '';
my %alldeps;
# Parse dependency files
while (<>) {
  s/\r?\n$//; # Handle both unix and DOS line endings
  $line .= $_;
  if ($line =~ /\\$/) {
    chop $line;
    next;
  }

  $line =~ s|\\|/|g;

  my ($obj,$rest) = split /\s*:\s+/, $line, 2;
  $line = '';
  next if !$obj || !$rest;

  my @deps = split /\s+/, $rest;
  push @{$alldeps{$obj}}, @deps;
  if (DEBUG >= 2) {
    foreach my $dep (@deps) { print STDERR "add $obj $dep\n"; }
  }
}

# Test dependencies
my %modtimes; # cache
my @objs;     # force rebuild on these
OBJ_LOOP: foreach my $obj (keys %alldeps) {
  my $mtime = (stat $obj)[9] or next;

  my %not_in_cache;
  my $deps = $alldeps{$obj};
  foreach my $dep_file (@{$deps}) {
    my $dep_mtime = $modtimes{$dep_file};
    if (not defined $dep_mtime) {
      print STDERR "Skipping $dep_file for $obj, will stat() later\n" if DEBUG >= 2;
      $not_in_cache{$dep_file} = 1;
      next;
    }

    print STDERR "Found $dep_file in cache\n" if DEBUG >= 2;

    if ($dep_mtime > $mtime) {
      print STDERR "$dep_file($dep_mtime) newer than $obj($mtime)\n" if DEBUG;
    }
    elsif ($dep_mtime == -1) {
      print STDERR "Couldn't stat $dep_file for $obj\n" if DEBUG;
    }
    else {
      print STDERR "$dep_file($dep_mtime) older than $obj($mtime)\n" if DEBUG >= 2;
      next;
    }

    push @objs, $obj; # dependency is missing or newer
    next OBJ_LOOP; # skip checking the rest of the dependencies
  }

  foreach my $dep_file (keys %not_in_cache) {
    print STDERR "STAT $dep_file for $obj\n" if DEBUG >= 2;
    my $dep_mtime = $modtimes{$dep_file} = (stat $dep_file)[9] || -1;

    if ($dep_mtime > $mtime) {
      print STDERR "$dep_file($dep_mtime) newer than $obj($mtime)\n" if DEBUG;
    }
    elsif ($dep_mtime == -1) {
      print STDERR "Couldn't stat $dep_file for $obj\n" if DEBUG;
    }
    else {
      print STDERR "$dep_file($dep_mtime) older than $obj($mtime)\n" if DEBUG >= 2;
      next;
    }

    push @objs, $obj; # dependency is missing or newer
    next OBJ_LOOP; # skip checking the rest of the dependencies
  }

  # If we get here it means nothing needs to be done for $obj
}

# Output objects to rebuild (if needed).
if ($outfile eq '-') {
    if (@objs) {
	print "@objs: FORCE\n";
    }
} elsif (@objs) {
  my $old_output;
  my $new_output = "@objs: FORCE\n";

  # Read in the current dependencies file.
  open(OLD, "<$outfile")
    and $old_output = <OLD>;
  close(OLD);

  # Only write out the dependencies if they are different.
  if ($new_output ne $old_output) {
    open(OUT, ">$outfile") and print OUT "$new_output";
    print "Updating dependencies file, $outfile\n" unless $silent;
    if (DEBUG) {
      print "new: $new_output\n";
      print "was: $old_output\n" if $old_output ne '';
    }
  }
} elsif (-s $outfile) {
  # Remove the old dependencies because all objects are up to date.
  print "Removing old dependencies file, $outfile\n" unless $silent;

  if (DEBUG) {
    my $old_output;
    open(OLD, "<$outfile")
      and $old_output = <OLD>;
    close(OLD);
    print "was: $old_output\n";
  }

  unlink $outfile;
}
