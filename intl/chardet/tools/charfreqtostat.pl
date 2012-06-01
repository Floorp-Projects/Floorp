#!/usr/bin/perl
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
sub GenNPL {
  my($ret) = << "END_NPL";
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
