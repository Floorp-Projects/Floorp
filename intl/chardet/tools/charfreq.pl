#!/usr/bin/perl
#!/usr/bin/perl 
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
open (STAT,$ARGV[0]) || die " cannot open data file $ARGV[0]\n";
@count;
while(<STAT>)
{
   @k = split(/\s+/, $_);
   $count{$k[0]} = $k[1];
}
$count = 0;
while(<STDIN>)
{
  @ck = split /\s*/, $_;
  $s = 0;
  $fb = 0;
  $cl = $#ck;
  $j = 0;
  while($j < $cl) {
     $cc = unpack("C", $ck[$j]);
     if(0 eq $s ) {
       if($cc > 0x80) {
         if($cc > 0xa0) {
           $fb = $ck[$j];
           $s = 2;
         } else {
           $s = 1;
         }
       } 
     } elsif (1 eq $s) {
     } else {
         if($cc > 0xa0) {
           $fb .= $ck[$j];
           $count{$fb}++;
           print $fb . " "  .$count{$fb} . "\n";
           $s = 0;
         } else {
           $s = 1;
         }
     }
     $j = $j + 1;
  }
}
foreach $c (sort(keys( %count )))
{
   print $c . " ". $count{$c} . "\n";
}
