#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@gb18030_cls);
my(@gb18030_st);
my($gb18030_ver);


@gb18030_cls = (
 [ 0x0e , 0x0f , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0x30 , 0x39 , 3 ],
 [ 0x00 , 0x3f , 1 ],
 [ 0x40 , 0x7e , 2 ],
 [ 0x7f , 0x7f , 4 ],
 [ 0x80 , 0x80 , 5 ],
 [ 0x81 , 0xfe , 6 ],
 [ 0xff , 0xff , 0 ],
);

package genverifier;
@gb18030_st = (
#  0  1  2  3  4  5  6 
   1, 0, 0, 0, 0, 0, 3, # state 0
   1, 1, 1, 1, 1, 1, 1, # Error State - 1
   2, 2, 2, 2, 2, 2, 2, # ItsMe State - 2
   1, 1, 0, 4, 1, 0, 0, # state 3, multibytes, 1st byte identified
   1, 1, 1, 1, 1, 1, 5, # state 4, multibytes, 2nd byte identified
   1, 1, 1, 2, 1, 1, 1, # state 5, multibytes, 3rd byte identified
);


$gb18030_ver = genverifier::GenVerifier("gb18030", "gb18030", \@gb18030_cls, 7,     \@gb18030_st);
print $gb18030_ver;



