#!/usr/bin/perl
#!/usr/bin/perl 
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
# This file is used to generate a EUC based character frequency map
# It will read in one character frequency map file
# Read in one new file , add the data to the frequency map
# And then updte the character frequency map to the stdout
# file format
# character count
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
