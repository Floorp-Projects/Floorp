#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@euctw_cls);
my(@euctw_st);
my($euctw_ver);


@euctw_cls = (
 [ 0x00 , 0x00 , 2 ],
 [ 0x0e , 0x0f , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0x01 , 0x7f , 2 ],
 [ 0x8e , 0x8e , 6 ],
 [ 0x80 , 0xa0 , 0 ],
 [ 0xff , 0xff , 0 ],
 [ 0xa1 , 0xa1 , 3 ],
 [ 0xa2 , 0xa7 , 4 ],
 [ 0xa8 , 0xa9 , 5 ],
 [ 0xaa , 0xc1 , 1 ],
 [ 0xc2 , 0xc2 , 3 ],
 [ 0xc3 , 0xc3 , 1 ],
 [ 0xc4 , 0xfe , 3 ],
);

package genverifier;
@euctw_st = (
#  0  1  2  3  4  5  6
   1, 1, 0, 3, 3, 3, 4, # state 0
   1, 1, 1, 1, 1, 1, 1, # Error State - 1
   2, 2, 2, 2, 2, 2, 2, # ItsMe State - 2
   1, 0, 1, 0, 0, 0, 1, # state 3
   1, 1, 1, 1, 5, 1, 1, # state 4
   1, 0, 1, 0, 0, 0, 1, # state 5
);


$euctw_ver = genverifier::GenVerifier("EUCTW", "x-euc-tw", \@euctw_cls, 7,     \@euctw_st);
print $euctw_ver;



