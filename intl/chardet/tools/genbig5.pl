#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@big5_cls);
my(@big5_st);
my($big5_ver);


@big5_cls = (
 [ 0x00 , 0x00 , 1 ],
 [ 0x0e , 0x0f , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0x01 , 0x3f , 1 ],
 [ 0x40 , 0x7e , 2 ],
 [ 0x7f , 0x7f , 1 ],
 [ 0xff , 0xff , 0 ],
 [ 0x80 , 0xa0 , 4 ],
 [ 0xa1 , 0xfe , 3 ],
);

package genverifier;
@big5_st = (
#  0  1  2  3  4 
   1, 0, 0, 3, 1, # state 0
   1, 1, 1, 1, 1, # Error State - 1
   2, 2, 2, 2, 2, # ItsMe State - 2
   1, 1, 0, 0, 0, # state 3
);


$big5_ver = genverifier::GenVerifier("BIG5", "Big5", \@big5_cls, 5,     \@big5_st);
print $big5_ver;



