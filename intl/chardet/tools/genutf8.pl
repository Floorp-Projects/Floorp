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
# UTF8 encode the UCS4 into 1 to 6 bytes
# 
# 1 byte    00 00 00 00   00 00 00 7f
# 2 bytes   00 00 00 80   00 00 07 ff
# 3 bytes   00 00 08 00   00 00 ff ff
# 4 bytes   00 01 00 00   00 1f ff ff
# 5 bytes   00 20 00 00   03 ff ff ff
# 6 bytes   04 00 00 00   7f ff ff ff
# 
# Howerver, since Surrogate area should not be encoded into UTF8 as
# a Surrogate pair, we can remove the surrogate area from UTF8
# 
# 1 byte    00 00 00 00   00 00 00 7f
# 2 bytes   00 00 00 80   00 00 07 ff
# 3 bytes   00 00 08 00   00 00 d7 ff
#           00 00 e0 00   00 00 ff ff
# 4 bytes   00 01 00 00   00 1f ff ff
# 5 bytes   00 20 00 00   03 ff ff ff
# 6 bytes   04 00 00 00   7f ff ff ff
# 
# Now we break them into 6 bits group for 2-6 bytes UTF8
# 
# 1 byte                   00                  7f
# 2 bytes               02 00               1f 3f
# 3 bytes            00 20 00            0d 1f 3f
#                    0e 00 00            0f 3f 3f
# 4 bytes         00 20 00 00         07 3f 3f 3f
# 5 bytes      00 08 00 00 00      03 3f 3f 3f 3f
# 6 bytes   00 04 00 00 00 00   01 3f 3f 3f 3f 3f
# 
# Break down more
# 
# 1 byte                   00                  7f
# 2 bytes               02 00               1f 3f
# 3 bytes            00 20 00            00 3f 3f
#                    01 00 00            0c 3f 3f
#                    0d 00 00            0d 1f 3f
#                    0e 00 00            0f 3f 3f
# 4 bytes         00 20 00 00         00 3f 3f 3f
#                 01 00 00 00         07 3f 3f 3f
# 5 bytes      00 08 00 00 00      00 3f 3f 3f 3f
#              01 00 00 00 00      03 3f 3f 3f 3f
# 6 bytes   00 04 00 00 00 00   00 3f 3f 3f 3f 3f
#           01 00 00 00 00 00   01 3f 3f 3f 3f 3f
# 
# Now, add 
#  c0 to the lead byte of 2 bytes UTF8
#  e0 to the lead byte of 3 bytes UTF8
#  f0 to the lead byte of 4 bytes UTF8
#  f8 to the lead byte of 5 bytes UTF8
#  fc to the lead byte of 6 bytes UTF8
#  80 to the trail bytes of 2 - 6 bytes UTF8
# 
# 1 byte                   00                  7f
# 2 bytes               c2 80               df bf
# 3 bytes            e0 a0 80            e0 bf bf
#                    e1 80 80            ec bf bf
#                    ed 80 80            ed 9f bf
#                    ee 80 80            ef bf bf
# 4 bytes         f0 a0 80 80         f0 bf bf bf
#                 f1 80 80 80         f7 bf bf bf
# 5 bytes      f8 88 80 80 80      f8 bf bf bf bf
#              f9 80 80 80 80      fb bf bf bf bf
# 6 bytes   fc 84 80 80 80 80   fc bf bf bf bf bf
#           fd 80 80 80 80 80   fd bf bf bf bf bf
# 
# 
# Now we can construct our state diagram
# 
# 0:0x00,0x0e,0x0f,0x1b->Error
# 0:[0-0x7f]->0
# 0:fd->3
# 0:fc->4
# 0:[f9-fb]->5
# 0:f8->6
# 0:[f1-f7]->7
# 0:f0->8
# 0:[e1-ecee-ef]->9
# 0:e0->10
# 0:ed->11
# 0:[c2-df]->12
# 0:*->Error
# 3:[80-bf]->5
# 3:*->Error
# 4:[84-bf]->5
# 4:*->Error
# 5:[80-bf]->7
# 5:*->Error
# 6:[88-bf]->7
# 6:*->Error
# 7:[80-bf]->9
# 7:*->Error
# 8:[a0-bf]->9
# 8:*->Error
# 9:[80-bf]->12
# 9:*->Error
# 10:[a0-bf]->12
# 10:*->Error
# 11:[80-9f]->12
# 11:*->Error
# 12:[80-bf]->0
# 12:*->Error
# 
# Now, we classified chars into class
# 
# 00,0e,0f,1b:k0
# 01-0d,10-1a,1c-7f:k1
# 80-83:k2
# 84-87:k3
# 88-9f:k4
# a0-bf:k5
# c0-c1:k0
# c2-df:k6
# e0:k7
# e1-ec:k8
# ed:k9
# ee-ef:k8
# f0:k10
# f1-f7:k11
# f8:k12
# f9-fb:k13
# fc:k14
# fd:k15
# fe-ff:k0
#
# Now, let's put them into array form

@utf8_cls = (
 [ 0x00 , 0x00 , 1 ],
 [ 0x0e , 0x0f , 0 ],
 [ 0x1b , 0x1b , 0 ],
 [ 0x01 , 0x0d , 1 ],
 [ 0x10 , 0x1a , 1 ],
 [ 0x1c , 0x7f , 1 ],
 [ 0x80 , 0x83 , 2 ],
 [ 0x84 , 0x87 , 3 ],
 [ 0x88 , 0x9f , 4 ],
 [ 0xa0 , 0xbf , 5 ],
 [ 0xc0 , 0xc1 , 0 ],
 [ 0xc2 , 0xdf , 6 ],
 [ 0xe0 , 0xe0 , 7 ],
 [ 0xe1 , 0xec , 8 ],
 [ 0xed , 0xed , 9 ],
 [ 0xee , 0xef , 8 ],
 [ 0xf0 , 0xf0 , 10 ],
 [ 0xf1 , 0xf7 , 11 ],
 [ 0xf8 , 0xf8 , 12 ],
 [ 0xf9 , 0xfb , 13 ],
 [ 0xfc , 0xfc , 14 ],
 [ 0xfd , 0xfd , 15 ],
 [ 0xfe , 0xff , 0 ],
);
# 
# Now, we write the state diagram in class 
# 
# 0:k0->Error
# 0:k1->0
# 0:k15->3
# 0:k14->4
# 0:k13->5
# 0:k12->6
# 0:k11->7
# 0:k10->8
# 0:k8->9
# 0:k7->10
# 0:k9->11
# 0:k6->12
# 0:*->Error
# 3:k2,k3,k4,k5->5
# 3:*->Error
# 4:k3,k4,k5->5
# 4:*->Error
# 5:k2,k3,k4,k5->7
# 5:*->Error
# 6:k4,k5->7
# 6:*->Error
# 7:k2,k3,k4,k5->9
# 7:*->Error
# 8:k5->9
# 8:*->Error
# 9:k2,k3,k4,k5->12
# 9:*->Error
# 10:k5->12
# 10:*->Error
# 11:k2,k3,k4->12
# 11:*->Error
# 12:k2,k3,k4,k5->0
# 12:*->Error
# 
# Now, let's put them into array
# 
package genverifier;
@utf8_st = (
# 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
  1, 0, 1, 1, 1, 1,12,10, 9,11, 8, 7, 6, 5, 4, 3, # state 0 Start
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 1 Error
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, # state 2 ItsMe
  1, 1, 5, 5, 5, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 3
  1, 1, 1, 5, 5, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 4
  1, 1, 7, 7, 7, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 5
  1, 1, 1, 1, 7, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 6
  1, 1, 9, 9, 9, 9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 7
  1, 1, 1, 1, 1, 9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 8
  1, 1,12,12,12,12, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 9
  1, 1, 1, 1, 1,12, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 10
  1, 1,12,12,12, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 11
  1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, # state 12
);



$utf8_ver = genverifier::GenVerifier("UTF8", "UTF-8", \@utf8_cls, 16,     \@utf8_st);
print $utf8_ver;



