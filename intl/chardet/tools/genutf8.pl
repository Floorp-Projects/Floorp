#!/usr/local/bin/perl
use strict;
require "genverifier.pm";
use genverifier;


my(@utf8_cls);
my(@utf8_st);
my($utf8_ver);

@utf8_cls = (
 [ 0x00 , 0x00 , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0xc0 , 0xc1 , 0 ],
 [ 0xfe , 0xfe , 0 ],
 [ 0xff , 0xff , 0 ],
 [ 0x01 , 0x1a , 1 ],
 [ 0x1c , 0x7f , 1 ],
 [ 0x80 , 0x81 , 2 ],
 [ 0x82 , 0x87 , 3 ],
 [ 0x88 , 0x8f , 4 ],
 [ 0x90 , 0x9f , 5 ],
 [ 0xa0 , 0xbf , 6 ],
 [ 0xc2 , 0xdf , 7 ],
 [ 0xe0 , 0xed , 8 ],
 [ 0xe1 , 0xec , 9 ],
 [ 0xee , 0xef , 9 ],
 [ 0xf0 , 0xf0 , 10 ],
 [ 0xf1 , 0xf7 , 11 ],
 [ 0xf8 , 0xf8 , 12 ],
 [ 0xf9 , 0xfb , 13 ],
 [ 0xfc , 0xfc , 14 ],
 [ 0xfd , 0xfd , 15 ]
);

package genverifier;
@utf8_st = (
# 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
  1, 0, 1, 1, 1, 1, 1, 3, 8, 4, 9, 5,10, 6, 7,11, # Start State - 0
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # Error State - 1
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, # ItsMe State - 2
  1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 3
  1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 4
  1, 1, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 5
  1, 1, 5, 5, 5, 5, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 6
  1, 1, 6, 6, 6, 6, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 7
  1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 8
  1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 9
  1, 1, 1, 1, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 10
  1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, #       State - 11
);

$utf8_ver = genverifier::GenVerifier("UTF8", "UTF-8", \@utf8_cls, 16,     \@utf8_st);
print $utf8_ver;



