#!env perl

# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/

# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2002 Netscape Communications Corporation. All
# Rights Reserved.

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

