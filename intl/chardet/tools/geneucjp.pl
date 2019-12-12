#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@eucjp_cls);
my(@eucjp_st);
my($eucjp_ver);


@eucjp_cls = (
 [ 0x0e , 0x0f , 5 ],
 [ 0xe0 , 0xfe , 0 ],
 [ 0x8e , 0x8e , 1 ],
 [ 0xa1 , 0xdf , 2 ],
 [ 0x8f , 0x8f , 3 ],
 [ 0x01 , 0x1a , 4 ],
 [ 0x1c , 0x7f , 4 ],
 [ 0x00 , 0x00 , 4 ],
 [ 0x1b , 0x1b , 5 ],
 [ 0x80 , 0x8d , 5 ],
 [ 0xa0 , 0xa0 , 5 ],
 [ 0x80 , 0xff , 5 ]
);

package genverifier;
@eucjp_st = (
#  0  1  2  3  4  5
   3, 4, 3, 5, 0, 1, # state 0
   1, 1, 1, 1, 1, 1, # Error State - 1
   2, 2, 2, 2, 2, 2, # ItsMe State - 2
   0, 1, 0, 1, 1, 1, # state 3
   1, 1, 0, 1, 1, 1, # state 4
   3, 1, 3, 1, 1, 1, # state 5
);


$eucjp_ver = genverifier::GenVerifier("EUCJP", "EUC-JP", \@eucjp_cls, 6,     \@eucjp_st);
print $eucjp_ver;



