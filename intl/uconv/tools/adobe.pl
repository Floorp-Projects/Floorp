#!/usr/local/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

while(<STDIN>)
{
  if(/^#/)
  {
      print $_;
  }
  else 
  {
     ($a, $b, $c,$d) = /^(....)(.)(..)(.*)$/;
     print "0x" . $c . $b . "0x" . $a .  $d . "\n";
  }
}
