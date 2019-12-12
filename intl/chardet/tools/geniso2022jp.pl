#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@iso2022jp_cls);
my(@iso2022jp_st);
my($iso2022jp_ver);

# 1:ESC 3:'(' 4:'B' 5:'J' 6:'@' 7:'$' 8:'D' 9:'I' 
@iso2022jp_cls = (
 [ 0x0e , 0x0f , 2 ],
 [ 0x28 , 0x28 , 3 ],
 [ 0x42 , 0x42 , 4 ],
 [ 0x4a , 0x4a , 5 ],
 [ 0x40 , 0x40 , 6 ],
 [ 0x24 , 0x24 , 7 ],
 [ 0x44 , 0x44 , 8 ],
 [ 0x49 , 0x49 , 9 ],
 [ 0x01 , 0x1a , 0 ],
 [ 0x1c , 0x7f , 0 ],
 [ 0x1b , 0x1b , 1 ],
 [ 0x00 , 0x00 , 2 ],
 [ 0x80 , 0xff , 2 ]
);

package genverifier;
@iso2022jp_st = (
# 0  1  2  3  4  5  6  7  8  9
  0, 3, 1, 0, 0, 0, 0, 0, 0, 0, # Start State - 0
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # Error State - 1
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, # ItsMe State - 2
  1, 1, 1, 5, 1, 1, 1, 4, 1, 1, # got ESC
  1, 1, 1, 6, 2, 1, 2, 1, 1, 1, # got ESC $
  1, 1, 1, 1, 2, 2, 1, 1, 1, 2, # got ESC (
  1, 1, 1, 1, 1, 1, 1, 1, 2, 1, # got ESC $ (
);

$iso2022jp_ver = genverifier::GenVerifier("ISO2022JP", "ISO-2022-JP", 
      \@iso2022jp_cls, 10, \@iso2022jp_st);
print $iso2022jp_ver;



