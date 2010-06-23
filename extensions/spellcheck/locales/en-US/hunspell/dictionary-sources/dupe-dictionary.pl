#!/usr/bin/perl
#
# dupe-dictionary.pl
#
# This will find all duplicate words in a myspell/hunspell format .dic file.
# It ignores affix rules, so 'One/ADG' = 'One' = 'One/12'
#
# Returns error if dupes are found

use strict;
use warnings;

my %seen;
my $bad = 0;

print "Duplicated entries:\n";

while (<>) {
    my $key = (split /([\n\/])/)[0];
    if (($key ne "") && (exists $seen{$key})) {
        $seen{$key}++;
        print "$key\n";
	# print ord($key);
	$bad++;
    } else {
        $seen{$key} = 1;
    }
}

if ($bad == 0) {
    print "None!\n";
} else {
       die "Duplicates found!";
}
