#!/usr/local/bin/perl
use strict;
require "genverifier.pm";
use genverifier;


my(@ucs2be_cls);
my(@ucs2be_st);
my($ucs2be_ver);


# We look at the following UCS2 char
# U+FEFF->ItsMe if it is the first Unicode
# U+FFFF->Error
# U+FFFE->Error
# U+0d0d->Error
# U+0a0d->Error
# U+1bxx->Error
# U+29xx->Error
# U+2axx->Error
# U+2bxx->Error
# U+2cxx->Error
# U+2dxx->Error
# 
# In UCS2-Big Endian it will be
# 
# Ev Od 
# FE FF->ItsMe for the first two bytes
# FF FF->Error
# FF FE->Error
# 0d 0d->Error
# 0a 0d->Error
# 1b   ->Error
# 29   ->Error
# 2a   ->Error
# 2b   ->Error
# 2c   ->Error
# 2d   ->Error
# 
# Now we classified the char
# 0a:k1
# 0d:k2
# 1b:k3
# 29-2d:k3
# fe:k4
# ff:k5
# others:k0
# 
@ucs2be_cls = (
 [ 0x0a, 0x0a, 1 ],
 [ 0x0d, 0x0d, 2 ],
 [ 0x1b, 0x1b, 3 ],
 [ 0x29, 0x2d, 3 ],
 [ 0xfe, 0xfe, 4 ],
 [ 0xff, 0xff, 5 ],
 [ 0x00, 0xff, 0 ]
);


# For Big Endian 
# 
# 0:k5->3
# 0:k4->4
# 0:k1,k2->7
# 0:k3->1
# 0:k0->5
# 3:k4,k5->1
# 3:*->6
# 4:k5->2
# 4:*->6
# 5:*->6
# 6:k1,k2->7
# 6:k3->1
# 6:k5->8
# 6:*->5
# 7:k2->1
# 7:*->6
# 8:k4,k5->1
# 8:*->6

package genverifier;
@ucs2be_st = (
# 0  1  2  3  4  5  
  5, 7, 7, 1, 4, 3,  # state 0 - Start
  1, 1, 1, 1, 1, 1,  # state 1 - Error
  2, 2, 2, 2, 2, 2,  # state 2 - ItsMe
  6, 6, 6, 6, 1, 1,  # state 3   1st byte got FF
  6, 6, 6, 6, 6, 2,  # state 4   1st byte got FE
  6, 6, 6, 6, 6, 6,  # state 5   Odd byte
  5, 7, 7, 1, 5, 8,  # state 6   Even Byte
  6, 6, 1, 6, 6, 6,  # state 7   Got 0A or 0D
  6, 6, 6, 6, 1, 1,  # state 8   Got FF
);


$ucs2be_ver = genverifier::GenVerifier("UCS2BE", "UTF-16BE", 
               \@ucs2be_cls, 6,     \@ucs2be_st);
print $ucs2be_ver;



