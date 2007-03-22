#!/usr/bin/perl
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
sub GenNPL {
  my($ret) = << "END_NPL";
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
END_NPL

  return $ret;
}

print GenNPL();
$total=0;
@h;
@l;

while(<STDIN>)
{
   @k = split(/\s+/, $_);
  @i = unpack("CCCC", $k[0]);
#  printf("%x %x %s",$i[0] ,  $i[1] , "[" . $k[0] . "]   " . $i . "  " . $j . "  " . $k[1]  ."\n");
  if((0xA1 <= $i[0]) && (0xA1 <= $i[1])){
  $total += $k[1];
     $v = $i[0] - 0x00A1;
     $h[$v] += $k[1];
     $u = $i[1] - 0x00A1;
     $l[$u] += $k[1];
#     print "hello $v $h[$v] $u $l[$u]\n";
  }
}


$ffh = 0.0;
$ffl = 0.0;
for($i=0x00A1;$i< 0x00FF ; $i++)
{
     $fh[$i - 0x00a1] = $h[$i- 0x00a1] / $total;
     $ffh += $fh[$i - 0x00a1];

     $fl[$i - 0x00a1] = $l[$i- 0x00a1] / $total;
     $ffl += $fl[$i - 0x00a1];
}
$mh = $ffh / 94.0;
$ml = $ffl / 94.0;

$sumh=0.0;
$suml=0.0;
for($i=0x00A1;$i< 0x00FF ; $i++)
{
     $sh = $fh[$i - 0x00a1] - $mh;
     $sh *= $sh;
     $sumh += $sh;

     $sl = $fl[$i - 0x00a1] - $ml;
     $sl *= $sl;
     $suml += $sl;
}
$sumh /= 94.0;
$suml /= 94.0;
$stdh = sqrt($sumh);
$stdl = sqrt($suml);

print "{\n";
print "  {\n";
for($i=0x00A1;$i< 0x00FF ; $i++)
{
   if($i eq 0xfe) {
     printf("   %.6ff  \/\/ FreqH[%2x]\n", $fh[$i - 0x00a1] , $i);
   } else {
     printf("   %.6ff, \/\/ FreqH[%2x]\n", $fh[$i - 0x00a1] , $i);
   }
}
print "  },\n";
printf ("%.6ff, \/\/ Lead Byte StdDev\n", $stdh);
printf ("%.6ff, \/\/ Lead Byte Mean\n", $mh);
printf ("%.6ff, \/\/ Lead Byte Weight\n", $stdh / ($stdh + $stdl));
print "  {\n";
for($i=0x00A1;$i< 0x00FF ; $i++)
{
   if($i eq 0xfe) {
     printf("  %.6ff  \/\/ FreqL[%2x]\n", $fl[$i - 0x00a1] ,  $i);
   } else {
     printf("  %.6ff, \/\/ FreqL[%2x]\n", $fl[$i - 0x00a1] ,  $i);
   }
}
print "  },\n";
printf ("%.6ff, \/\/ Trail Byte StdDev\n", $stdl);
printf ("%.6ff, \/\/ Trail Byte Mean\n", $ml);
printf ("%.6ff  \/\/ Trial Byte Weight\n", $stdl / ($stdh + $stdl));
print "};\n";
