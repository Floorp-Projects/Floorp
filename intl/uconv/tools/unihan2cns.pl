#!/usr/local/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use IO::File;
my(%tagtofilemap);
$tagtofilemap{"kCNS1986-1" } = IO::File->new("|sort> cns1986p1.txt")  
      or die "cannot open cns1986p1.txt";
$tagtofilemap{"kCNS1986-2" } = IO::File->new("|sort> cns1986p2.txt")  
      or die "cannot open cns1986p2.txt";
$tagtofilemap{"kCNS1986-E" } = IO::File->new("|sort> cns1986p14.txt") 
      or die "cannot open cns1986p14.txt"; 
$tagtofilemap{"kCNS1992-1" } = IO::File->new("|sort> cns1992p1.txt")  
      or die "cannot open cns1992p1.txt"; 
$tagtofilemap{"kCNS1992-2" } = IO::File->new("|sort> cns1992p2.txt") 
      or die "cannot open cns1992p2.txt"; 
$tagtofilemap{"kCNS1992-3" } = IO::File->new("|sort> cns1992p3.txt") 
      or die "cannot open cns1992p3.txt"; 
$tagtofilemap{"kIRG_TSource-1" } = IO::File->new("|sort> cnsIRGTp1.txt") 
      or die "cannot open cnsIRGTp1.txt"; 
$tagtofilemap{"kIRG_TSource-2" } = IO::File->new("|sort> cnsIRGTp2.txt") 
      or die "cannot open cnsIRGTp2.txt"; 
$tagtofilemap{"kIRG_TSource-3" } = IO::File->new("|sort> cnsIRGTp3.txt") 
      or die "cannot open cnsIRGTp3.txt"; 
$tagtofilemap{"kIRG_TSource-4" } = IO::File->new("|sort> cnsIRGTp4.txt") 
      or die "cannot open cnsIRGTp4.txt"; 
$tagtofilemap{"kIRG_TSource-5" } = IO::File->new("|sort> cnsIRGTp5.txt") 
      or die "cannot open cnsIRGTp5.txt"; 
$tagtofilemap{"kIRG_TSource-6" } = IO::File->new("|sort> cnsIRGTp6.txt") 
      or die "cannot open cnsIRGTp6.txt"; 
$tagtofilemap{"kIRG_TSource-7" } = IO::File->new("|sort> cnsIRGTp7.txt") 
      or die "cannot open cnsIRGTp7.txt"; 
$tagtofilemap{"kIRG_TSource-F" } = IO::File->new("|sort> cnsIRGTp15.txt") 
      or die "cannot open cnsIRGTp15.txt";
$tagtofilemap{"kIRG_TSource-3ExtB" } = IO::File->new("|sort> cnsIRGTp3ExtB.txt") 
      or die "cannot open cnsIRGTp3ExtB.txt"; 
$tagtofilemap{"kIRG_TSource-4ExtB" } = IO::File->new("|sort> cnsIRGTp4ExtB.txt") 
      or die "cannot open cnsIRGTp4ExtB.txt"; 
$tagtofilemap{"kIRG_TSource-5ExtB" } = IO::File->new("|sort> cnsIRGTp5ExtB.txt") 
      or die "cannot open cnsIRGTp5ExtB.txt"; 
$tagtofilemap{"kIRG_TSource-6ExtB" } = IO::File->new("|sort> cnsIRGTp6ExtB.txt") 
      or die "cannot open cnsIRGTp6ExtB.txt"; 
$tagtofilemap{"kIRG_TSource-7ExtB" } = IO::File->new("|sort> cnsIRGTp7ExtB.txt") 
      or die "cannot open cnsIRGTp7ExtB.txt"; 
$tagtofilemap{"kIRG_TSource-FExtB" } = IO::File->new("|sort> cnsIRGTp15ExtB.txt") 
      or die "cannot open cnsIRGTp15ExtB.txt";

$nonhan =  IO::File->new("< nonhan.txt") 
      or die "cannot open nonhan.txt";

while(defined($line = $nonhan->getline()))
{
   $tagtofilemap{"kCNS1986-1"}->print($line);
   $tagtofilemap{"kCNS1992-1"}->print($line);
   $tagtofilemap{"kIRG_TSource-1"}->print($line);
}

while(<STDIN>)
{
  if(/^U/)
  {
    chop();
    ($u,$tag,$value) = split(/\t/,$_);
    if($tag =~ m/(kCNS|kIRG_TSource)/)
    {
         ($pnum, $cvalue) = split(/-/,$value);
         $tagkey = $tag . "-" . $pnum;
         if(length($u) > 6) {
           $tagkey .= "ExtB";
         }
         $fd = $tagtofilemap{$tagkey};
         if(length($u) > 6) {
           $mapping = substr($u,3,4); # trunkcate 0x2 from 0x2abcd
         } else {
           $mapping = substr($u,2,4); # trunkcate 0x from 0xabcd
         }
         $fd->print("0x" . $cvalue . "\t0x" . $mapping . "\t# <CJK>\n");
    }
  }
}

