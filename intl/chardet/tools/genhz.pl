#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@hz_cls);
my(@hz_st);
my($hz_ver);


#
#
# > 0x80  - 1
# ~       - 2
# LF      - 3
# {       - 4
# }       - 5
#
@hz_cls = (
 [ 0x01 , 0x1a , 0 ],
 [ 0x7e , 0x7e , 2 ],
 [ 0x0a , 0x0a , 3 ],
 [ 0x7b , 0x7b , 4 ],
 [ 0x7d , 0x7d , 5 ],
 [ 0x1c , 0x7f , 0 ],
 [ 0x0e , 0x0f , 1 ],
 [ 0x1b , 0x1b , 1 ],
 [ 0x00 , 0x00 , 1 ],
 [ 0x80 , 0xff , 1 ]
);


#
#
package genverifier;
@hz_st = (
# 0  1  2  3  4  5 
  0, 1, 3, 0, 0, 0, # Start State - 0
  1, 1, 1, 1, 1, 1, # Error State - 1
  2, 2, 2, 2, 2, 2, # ItsMe State - 2
  1, 1, 0, 0, 4, 1, # state 3 - got ~
  5, 1, 6, 1, 5, 5, # state 4 - got ~ {
  4, 1, 4, 1, 4, 4, # state 5 - got ~ { X
  4, 1, 4, 1, 4, 2, # state 6 - got ~ { [X X]* ~
);

$hz_ver = genverifier::GenVerifier("HZ", "HZ-GB-2312", 
      \@hz_cls, 6, \@hz_st);
print $hz_ver;



