#!/usr/local/bin/perl
use strict;
require "genverifier.pm";
use genverifier;


my(@ucs2be_cls);
my(@ucs2be_st);
my($ucs2be_ver);


@ucs2be_cls = (
 [ 0x00 , 0x00 , 1 ],
 [ 0x0a , 0x0a , 4 ],
 [ 0x0d , 0x0d , 3 ],
 [ 0x28 , 0x2f , 0 ],
 [ 0x34 , 0x39 , 0 ],
 [ 0x41 , 0x4d , 0 ],
 [ 0x80 , 0x9f , 2 ],
 [ 0xfe , 0xfe , 5 ],
 [ 0xff , 0xff , 6 ],
 [ 0x00 , 0xfd , 7 ],
);

package genverifier;
@ucs2be_st = (
#  0  1  2  3  4  5  6  7
   1, 5, 3, 6, 6, 7, 8, 3, # state 0
   1, 1, 1, 1, 1, 1, 1, 1, # Error State - 1
   2, 2, 2, 2, 2, 2, 2, 2, # ItsMe State - 2
   4, 4, 4, 4, 4, 4, 4, 4, # state 3
   1, 5, 3, 6, 6, 3, 3, 3, # state 4
   4, 4, 1, 4, 4, 4, 4, 4, # state 5
   4, 4, 4, 1, 4, 4, 4, 4, # state 6
   4, 4, 4, 4, 4, 4, 2, 4, # state 7
   4, 4, 4, 4, 4, 1, 4, 4, # state 8
);


$ucs2be_ver = genverifier::GenVerifier("UCS2BE", "ISO-10646-UCS-2", \@ucs2be_cls, 8,     \@ucs2be_st);
print $ucs2be_ver;



