#!/usr/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


#
# Create a mapping from symbolic component name to directory name(s).
#
# Tue Oct 16 16:48:36 PDT 2001
# <mcafee@netscape.com>

use strict;

# For --option1, --option2, ...
use Getopt::Long;
Getopt::Long::Configure("bundling_override");
Getopt::Long::Configure("auto_abbrev");

# Globals
my $list_only_mode = 0;
my $opt_list_only;
my $mapfile = "";
my %map;

sub PrintUsage {
  die <<END_USAGE
  Prints out directories needed for a given list of components.
  usage: module2dir.pl [--list-only] [--mapfile mapfile] <component-name1> <component-name2> ...
END_USAGE
}

sub parse_map_file($) {
    my ($mapfile) = @_;
    my (%mod_map, $tmp, $dir, $mod, @mod_list);

    undef %mod_map;
    open (MAPFILE, "$mapfile") || die ("$mapfile: $!\n");
    while ($tmp=<MAPFILE>) {
	chomp ($tmp);
	($dir, $mod, @mod_list) = split(/:/, $tmp, 3);
	$mod =~ s/[\s]*(\S+)[\s]*/$1/;
	$mod_map{$mod} .= "$dir ";
    }
    close(MAPFILE);
    foreach $mod (sort(keys %mod_map)) {
	my (@dirlist, @trimlist, $found, $tdir);
	@dirlist = split(/\s+/, $mod_map{$mod});
	$mod_map{$mod} = "";
	foreach $dir (@dirlist) {
	    $found = 0; 
	    foreach $tdir (@trimlist) {
		$found++, last if ($dir =~ m/^$tdir\// || $dir eq $tdir);
	    }
	    push @trimlist, $dir if (!$found);
        }
	$map{$mod} = join(" ", @trimlist);
	#print "$mod: $map{$mod}\n";
    }
}

sub dir_for_required_component {
  my ($component) = @_;
  my $rv;
  my $dir;

  $dir = $map{$component};
  if($dir) {
	# prepend "mozilla/" in front of directory names.
	$rv = "mozilla/$dir";
	$rv =~ s/\s+/ mozilla\//g;  # Hack for 2 or more directories.
  } else {
	$rv = 0;
  }
  return $rv;
}

{

  # Add stdin to the commandline.  This makes commandline-only mode hang,
  # call it a bug.  Not sure how to get around this.
  push (@ARGV, split(' ',<STDIN>));

  PrintUsage() if !GetOptions('list-only' => \$opt_list_only,
			      'mapfile=s' => \$mapfile);

  # Pick up arguments, if any.
  if($opt_list_only) {
  	$list_only_mode = 1;
  }

  &parse_map_file($mapfile);

  my $arg;
  my $dir;
  while ($arg = shift @ARGV) {
	$dir = dir_for_required_component($arg);
	if($dir) {
      if($list_only_mode) {
		print $dir, " ";
	  } else {
		print "$arg: ", $dir, "\n";
	  }
	} else {
	  # do nothing
	}
  }
  if($dir && $list_only_mode) {
	print "\n";
  }
}
