#!/usr/local/bin/perl
use strict;
require "genverifier.pm";
use genverifier;


my(@ucs2le_cls);
my(@ucs2le_st);
my($ucs2le_ver);

# We look at the following UCS2
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
# In UCS2-Little Endian it will be
# 
# Ev Od
# FF FE->ItsMe for the first two bytes
# FF FF->Error
# FE FF->Error
# 0d 0d->Error
# 0d 0a->Error
#    1b->Error
#    29->Error
#    2a->Error
#    2b->Error
#    2c->Error
#    2d->Error
# 
# Now we classified the char
# 0a:k1
# 0d:k2
# 1b:k3
# 29-2d:k3
# fe:k4
# ff:k5
# others:k0

@ucs2le_cls = (
 [ 0x0a, 0x0a, 1 ],
 [ 0x0d, 0x0d, 2 ],
 [ 0x1b, 0x1b, 3 ],
 [ 0x29, 0x2d, 3 ],
 [ 0xfe, 0xfe, 4 ],
 [ 0xff, 0xff, 5 ],
 [ 0x00, 0xff, 0 ]
);


# For Little Endian 
# 0:k5->3
# 0:k4->4
# 0:k2->7
# 0:*->6
# 3:k4->2
# 3:k5->1
# 3:k3->1
# 3:*->5
# 4:k3,k5->1
# 4:*->5
# 5:k4,k5->8
# 5:k2->7
# 5:*->6
# 6:k3->1
# 6:*->5
# 7:k1,k2->1
# 7:k3->1
# 7:*->5
# 8:k5->1
# 8:k3->1
# 8:*->5


package genverifier;
@ucs2le_st = (
# 0  1  2  3  4  5  
  6, 6, 7, 6, 4, 3, # state 0 - Start
  1, 1, 1, 1, 1, 1, # state 1 - Error
  2, 2, 2, 2, 2, 2, # state 2 - ItsMe
  5, 5, 5, 1, 2, 1, # state 3   1st byte Got FF 
  5, 5, 5, 1, 5, 1, # state 4   1st byte Got FE
  6, 6, 7, 6, 8, 8, # state 5   Even Byte
  5, 5, 5, 1, 5, 5, # state 6   Odd Byte
  5, 1, 1, 1, 5, 5, # state 7   Got 0d
  5, 5, 5, 1, 5, 1, # state 8   Got FF
);


$ucs2le_ver = genverifier::GenVerifier("UCS2LE", "UTF-16LE", \@ucs2le_cls,
              6,     \@ucs2le_st);
print $ucs2le_ver;



