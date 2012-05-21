#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@cp1252_cls);
my(@cp1252_st);
my($cp1252_ver);


@cp1252_cls = (
 [ 0x00 , 0x00 , 1 ],
 [ 0x0e , 0x0f , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0x81 , 0x81 , 0 ],
 [ 0x8d , 0x8d , 0 ],
 [ 0x8f , 0x8f , 0 ],
 [ 0x90 , 0x90 , 0 ],
 [ 0x9d , 0x9d , 0 ],
 [ 0xc0 , 0xd6 , 1 ],
 [ 0xd8 , 0xf6 , 1 ],
 [ 0xf8 , 0xff , 1 ],
 [ 0x8a , 0x8a , 1 ],
 [ 0x8c , 0x8c , 1 ],
 [ 0x8e , 0x8e , 1 ],
 [ 0x9a , 0x9a , 1 ],
 [ 0x9c , 0x9c , 1 ],
 [ 0x9e , 0x9e , 1 ],
 [ 0x9f , 0x9f , 1 ],
 [ 0x00 , 0xff , 2 ],
);

package genverifier;
@cp1252_st = (
# 0  1  2  
  1, 3, 0,  # Start State - 0
  1, 1, 1,  # Error State - 1
  2, 2, 2,  # ItsMe State - 2
  1, 4, 0,  #       State - 3
  1, 5, 4,  #       State - 4
  1, 1, 4,  #       State - 5
);


$cp1252_ver = genverifier::GenVerifier("CP1252", "windows-1252", 
         \@cp1252_cls, 3,     \@cp1252_st);
print $cp1252_ver;



