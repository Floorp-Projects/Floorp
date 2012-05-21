#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
