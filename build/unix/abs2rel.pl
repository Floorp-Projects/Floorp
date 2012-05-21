#!env perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use File::Spec::Unix;
use strict;

print "Usage: $0 dest_path start_path\n" if ($#ARGV+1 != 2);
my $finish = my_canonpath(shift);
my $start = my_canonpath(shift);

my $res = File::Spec::Unix->abs2rel($finish, $start);

#print STDERR "abs2rel($finish,$start) = $res\n";
print "$res\n";

sub my_canonpath($) {
    my ($file) = @_;
    my (@inlist, @outlist, $dir);

    # Do what File::Spec::Unix->no_upwards should do
    my @inlist = split(/\//, File::Spec::Unix->canonpath($file));
    foreach $dir (@inlist) {
	if ($dir eq '..') {
	    pop @outlist;
	} else {
	    push @outlist, $dir;
	}
    }
    $file = join '/',@outlist;
    return $file;
}

