#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@sjis_cls);
my(@sjis_st);
my($sjis_ver);

@sjis_cls = (
 [ 0x00 , 0x00 , 0 ],
 [ 0x0e , 0x0f , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0xfd , 0xff , 0 ],
 [ 0x85 , 0x86 , 3 ],
 [ 0xeb , 0xec , 5 ],
 [ 0x01 , 0x1a , 1 ],
 [ 0x1c , 0x3f , 1 ],
 [ 0x7f , 0x7f , 1 ],
 [ 0x40 , 0x7e , 2 ],
 [ 0xa1 , 0xdf , 2 ],
 [ 0x80 , 0x9f , 3 ],
 [ 0xa0 , 0xa0 , 4 ],
 [ 0xe0 , 0xea , 3 ],
 [ 0xed , 0xfc , 4 ],
);

package genverifier;
@sjis_st = (
# 0  1  2  3  4  5
  1, 0, 0, 3, 1, 1, # Start State - 0
  1, 1, 1, 1, 1, 1, # Error State - 1
  2, 2, 2, 2, 2, 2, # ItsMe State - 2
  1, 1, 0, 0, 0, 0, #       State - 3
);

$sjis_ver = genverifier::GenVerifier("SJIS", "Shift_JIS", \@sjis_cls, 6,     \@sjis_st);
print $sjis_ver;



