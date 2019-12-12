#!/usr/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
require "genverifier.pm";
use genverifier;


my(@utf8_cls);
my(@utf8_st);
my($utf8_ver);

#
#
# UTF8 encode the UCS4 into 1 to 4 bytes
#
# 1 byte    00 00 00 00   00 00 00 7f
# 2 bytes   00 00 00 80   00 00 07 ff
# 3 bytes   00 00 08 00   00 00 ff ff
# 4 bytes   00 01 00 00   00 10 ff ff
#
# However, since Surrogate area should not be encoded into UTF8 as
# a Surrogate pair, we can remove the surrogate area from UTF8
#
# 1 byte    00 00 00 00   00 00 00 7f
# 2 bytes   00 00 00 80   00 00 07 ff
# 3 bytes   00 00 08 00   00 00 d7 ff
#           00 00 e0 00   00 00 ff ff
# 4 bytes   00 01 00 00   00 10 ff ff
#
# Now we break them into 6 bits group for 2-4 bytes UTF8
#
# 1 byte                   00                  7f
# 2 bytes               02 00               1f 3f
# 3 bytes            00 20 00            0d 1f 3f
#                    0e 00 00            0f 3f 3f
# 4 bytes         00 10 00 00         04 0f 3f 3f
#
# Break down more
#
# 1 byte                   00                  7f
# 2 bytes               02 00               1f 3f
# 3 bytes            00 20 00            00 3f 3f
#                    01 00 00            0c 3f 3f
#                    0d 00 00            0d 1f 3f
#                    0e 00 00            0f 3f 3f
# 4 bytes         00 10 00 00         00 3f 3f 3f
#                 01 00 00 00         03 3f 3f 3f
#                 04 00 00 00         04 0f 3f 3f
#
# Now, add
#  c0 to the lead byte of 2 bytes UTF8
#  e0 to the lead byte of 3 bytes UTF8
#  f0 to the lead byte of 4 bytes UTF8
#  80 to the trail bytes
#
# 1 byte                   00                  7f
# 2 bytes               c2 80               df bf
# 3 bytes            e0 a0 80            e0 bf bf
#                    e1 80 80            ec bf bf
#                    ed 80 80            ed 9f bf
#                    ee 80 80            ef bf bf
# 4 bytes         f0 90 80 80         f0 bf bf bf
#                 f1 80 80 80         f3 bf bf bf
#                 f4 80 80 80         f4 8f bf bf
#
#
# Now we can construct our state diagram
#
# 0:0x0e,0x0f,0x1b->Error
# 0:[0-0x7f]->0
# 0:[c2-df]->3
# 0:e0->4
# 0:[e1-ec, ee-ef]->5
# 0:ed->6
# 0:f0->7
# 0:[f1-f3]->8
# 0:f4->9
# 0:*->Error
# 3:[80-bf]->0
# 3:*->Error
# 4:[a0-bf]->3
# 4:*->Error
# 5:[80-bf]->3
# 5:*->Error
# 6:[80-9f]->3
# 6:*->Error
# 7:[90-bf]->5
# 7:*->Error
# 8:[80-bf]->5
# 8:*->Error
# 9:[80-8f]->5
# 9:*->Error
#
# Now, we classified chars into class
#
# 00,0e,0f,1b:k0
# 01-0d,10-1a,1c-7f:k1
# 80-8f:k2
# 90-9f:k3
# a0-bf:k4
# c0-c1:k0
# c2-df:k5
# e0:k6
# e1-ec:k7
# ed:k8
# ee-ef:k7
# f0:k9
# f1-f3:k10
# f4:k11
# f5-ff:k0
#
# Now, let's put them into array form

@utf8_cls = (
 [ 0x00 , 0x00 , 1 ],
 [ 0x0e , 0x0f , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0x01 , 0x0d , 1 ],
 [ 0x10 , 0x1a , 1 ],
 [ 0x1c , 0x7f , 1 ],
 [ 0x80 , 0x8f , 2 ],
 [ 0x90 , 0x9f , 3 ],
 [ 0xa0 , 0xbf , 4 ],
 [ 0xc0 , 0xc1 , 0 ],
 [ 0xc2 , 0xdf , 5 ],
 [ 0xe0 , 0xe0 , 6 ],
 [ 0xe1 , 0xec , 7 ],
 [ 0xed , 0xed , 8 ],
 [ 0xee , 0xef , 7 ],
 [ 0xf0 , 0xf0 , 9 ],
 [ 0xf1 , 0xf3 , 10 ],
 [ 0xf4 , 0xf4 , 11 ],
 [ 0xf5 , 0xff , 0 ],
);
#
# Now, we write the state diagram in class
#
# 0:k0->Error
# 0:k1->0
# 0:k5->3
# 0:k6->4
# 0:k7->5
# 0:k8->6
# 0:k9->7
# 0:k10->8
# 0:k11->9
# 0:*->Error
# 3:k2,k3,k4->0
# 3:*->Error
# 4:k4->3
# 4:*->Error
# 5:k2,k3,k4->3
# 5:*->Error
# 6:k2,k3->3
# 6:*->Error
# 7:k3,k4->5
# 7:*->Error
# 8:k2,k3,k4->5
# 8:*->Error
# 9:k2->5
# 9:*->Error
#
# Now, let's put them into array
#
package genverifier;
@utf8_st = (
# 0  1  2  3  4  5  6  7  8  9 10 11
  1, 0, 1, 1, 1, 3, 4, 5, 6, 7, 8, 9, # state 0 Start
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 1 Error
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, # state 2 ItsMe
  1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, # state 3
  1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, # state 4
  1, 1, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, # state 5
  1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, # state 6
  1, 1, 5, 5, 1, 1, 1, 1, 1, 1, 1, 1, # state 7
  1, 1, 5, 5, 5, 1, 1, 1, 1, 1, 1, 1, # state 8
  1, 1, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 9
);



$utf8_ver = genverifier::GenVerifier("UTF8", "UTF-8", \@utf8_cls, 12,     \@utf8_st);
print $utf8_ver;



