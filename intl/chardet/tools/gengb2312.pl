#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@gb2312_cls);
my(@gb2312_st);
my($gb2312_ver);


@gb2312_cls = (
 [ 0x00 , 0x00 , 1 ],
 [ 0x0e , 0x0f , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0x01 , 0x7f , 1 ],
 [ 0x80 , 0xa0 , 0 ],
 [ 0xff , 0xff , 0 ],
 [ 0xaa , 0xaf , 3 ],
 [ 0xa1 , 0xfe , 2 ],
);

package genverifier;
@gb2312_st = (
#  0  1  2  3  
   1, 0, 3, 1,  # state 0
   1, 1, 1, 1,  # Error State - 1
   2, 2, 2, 2,  # ItsMe State - 2
   1, 1, 0, 0,  # state 3
);


$gb2312_ver = genverifier::GenVerifier("GB2312", "GB2312", \@gb2312_cls, 4,     \@gb2312_st);
print $gb2312_ver;



