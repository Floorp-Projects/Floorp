#!/usr/local/bin/perl
use strict;
require "genverifier.pm";
use genverifier;


my(@iso2022jp_cls);
my(@iso2022jp_st);
my($iso2022jp_ver);

@iso2022jp_cls = (
 [ 0x01 , 0x1a , 0 ],
 [ 0x1c , 0x7f , 0 ],
 [ 0x1b , 0x1b , 1 ],
 [ 0x00 , 0x00 , 2 ],
 [ 0x80 , 0xff , 2 ]
);

package genverifier;
@iso2022jp_st = (
# 0  1  2   
  0, 1, 2,   # Start State - 0
  1, 1, 1,   # Error State - 1
  2, 2, 2,   # ItsMe State - 2
);

$iso2022jp_ver = genverifier::GenVerifier("ISO2022JP", "ISO-2022-JP", \@iso2022jp_cls, 3, \@iso2022jp_st);
print $iso2022jp_ver;



