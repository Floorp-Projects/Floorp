#!/usr/bin/env perl

# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is this file as it was released upon December 26, 2000.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 2000 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#         Christopher Seawood <cls@seawood.org>

use Getopt::Std;

getopts('rs');
$regexp = 1 if (defined($opt_r));
$sort = 1 if (defined($opt_s));

undef @out;
if ($sort) {
    @in = sort @ARGV;
} else {
    @in = @ARGV;
}
foreach $d (@in) { 
    if ($regexp) {
        $found = 0; 
        foreach $dir (@out) {
            $found++, last if ($d =~ m/^$dir\// || $d eq $dir);
        }
        push @out, $d if (!$found);
    } else {
        push @out, $d if (!grep(/^$d$/, @out));
    }
}
print "@out\n"
