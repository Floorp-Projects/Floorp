#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@iso2022cn_cls);
my(@iso2022cn_st);
my($iso2022cn_ver);


#
#
# ESC     - 1
# > 0x80  - 2
# $       - 3
# )       - 4
# *       - 5
# A G     - 6
# H       - 7
# N O     - 8
#
@iso2022cn_cls = (
 [ 0x01 , 0x1a , 0 ],
 [ 0x29 , 0x29 , 3 ],
 [ 0x43 , 0x43 , 4 ],
 [ 0x1c , 0x7f , 0 ],
 [ 0x1b , 0x1b , 1 ],
 [ 0x00 , 0x00 , 2 ],
 [ 0x80 , 0xff , 2 ]
);


#
#  ESC$((([)][AG])|([*]H))|[NO])
#
package genverifier;
@iso2022cn_st = (
# 0  1  2  3  4  5  6  7  8
  0, 3, 1, 0, 0, 0, 0, 0, 0, # Start State - 0
  1, 1, 1, 1, 1, 1, 1, 1, 1, # Error State - 1
  2, 2, 2, 2, 2, 2, 2, 2, 2, # ItsMe State - 2
  1, 1, 1, 4, 1, 1, 1, 1, 2, # state 3 - got ESC
  1, 1, 1, 1, 5, 6, 1, 1, 1, # state 4 - got ESC $
  1, 1, 1, 1, 1, 1, 2, 1, 1, # state 5 - got ESC $ )
  1, 1, 1, 1, 1, 1, 1, 2, 1, # state 6 - got ESC $ *
);

$iso2022cn_ver = genverifier::GenVerifier("ISO2022CN", "ISO-2022-CN", 
      \@iso2022cn_cls, 9, \@iso2022cn_st);
print $iso2022cn_ver;



