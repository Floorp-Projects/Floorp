#!/usr/bin/perl
# parse-mozilla-encoding-table.pl, version 0.6
#
# Script to deassemble existing Mozilla *.uf or *.ut files
# back to source conversion tables.
# by Anthony Fok <anthony@thizlinux.com>, ThizLinux Laboratory Ltd., 2002/11/27
# License: GNU General Public License, version 2 or newer
# 
# Used for verifying HKSCS-1999 hkscs.uf and hkscs.ut so that I can make
# new ones for HKSCS-2001.  This script is quick-and-dirty and not very
# robust, so if the debug output of fromu/tou ever changes, this script
# will need to be modified too.  :-)

my %data = ();
my $mappingPos = 0;
my $filename = shift;
my $mode;
if ($filename =~ /\.(ut|uf)$/) {
  print $filename, "\n";
  $mode = $1;
} else {
  die;
}

open(INFILE, "<$filename") or die;

# Quick-and-dirty routine to populate %data
while (<INFILE>) {
  if (/^Begin of Item ([[:xdigit:]]+)/) {
    die if defined($itemId) and hex($itemId) + 1 != hex($1);
    $itemId = $1;
    <INFILE> =~ /Format ([012])/ or die;
    $format = $1;
    <INFILE> =~ /srcBegin = ([[:xdigit:]]+)/ or die;
    $srcBegin = $1;
    
    if ($format == 0) {		# Range
      <INFILE> =~ /srcEnd = ([[:xdigit:]]+)/ or die;
      $srcEnd = $1;
      <INFILE> =~ /destBegin = ([[:xdigit:]]+)/ or die;
      $destBegin = $1;

      for ($i = hex($srcBegin); $i <= hex($srcEnd); $i++) {
        $data{sprintf("%04X",$i)} = sprintf("%04X",
	    hex($destBegin) + $i - hex($srcBegin));
      }

      <INFILE> =~ /^End of Item $itemId\s*$/ or die;
    }
    elsif ($format == 1) {	# Mapping
      <INFILE> =~ /srcEnd = ([[:xdigit:]]+)/ or die;
      $srcEnd = $1;
      <INFILE> =~ /mappingOffset = ([[:xdigit:]]+)/ or die;
      $mappingOffset = hex($1);
      die unless $mappingOffset == $mappingPos;
      <INFILE> =~ /Mapping  =\s*$/ or die;
      until ($_ = <INFILE>, /^End of Item/) {
        chop;
        for $i (split ' ') {
	  $key = sprintf("%04X", hex($srcBegin) - $mappingOffset + $mappingPos++);
	  next if $i eq "FFFD";
	  if (defined($data{$key})) {
	    print "Error: doubly defined. $key was $data{$key}, and now $i.\n";
	  } else {
	    $data{$key} = $i;
	  }
	}
      }
      die unless $mappingPos - $mappingOffset == hex($srcEnd) - hex($srcBegin) + 1;
      /^End of Item $itemId\s*$/ or die;
    }
    else {			# Single ($format == 2)
      <INFILE> =~ /destBegin = ([[:xdigit:]]+)/ or die;
      $destBegin = $1;
      $data{$srcBegin} = $destBegin;
      <INFILE> =~ /^End of Item $itemId\s*$/ or die;
    }
  }
}

# Generate conversion table
for $key (sort keys %data) {
  if ($mode eq "ut") {
    print "0x$key\t0x$data{$key}\n";
  } elsif ($mode eq "uf") {
    print "0x$data{$key}\t0x$key\n";
  } else {
    die;
  }
}

close INFILE;
