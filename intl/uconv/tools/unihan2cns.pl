#!/usr/local/bin/perl -w
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
      or die "cannot open IRGTp1.txt"; 
$tagtofilemap{"kIRG_TSource-2" } = IO::File->new("|sort> cnsIRGTp2.txt") 
      or die "cannot open IRGTp2.txt"; 
$tagtofilemap{"kIRG_TSource-3" } = IO::File->new("|sort> cnsIRGTp3.txt") 
      or die "cannot open IRGTp3.txt"; 
$tagtofilemap{"kIRG_TSource-4" } = IO::File->new("|sort> cnsIRGTp4.txt") 
      or die "cannot open IRGTp4.txt"; 
$tagtofilemap{"kIRG_TSource-5" } = IO::File->new("|sort> cnsIRGTp5.txt") 
      or die "cannot open IRGTp5.txt"; 
$tagtofilemap{"kIRG_TSource-6" } = IO::File->new("|sort> cnsIRGTp6.txt") 
      or die "cannot open IRGTp6.txt"; 
$tagtofilemap{"kIRG_TSource-7" } = IO::File->new("|sort> cnsIRGTp7.txt") 
      or die "cannot open IRGTp7.txt"; 
$tagtofilemap{"kIRG_TSource-F" } = IO::File->new("|sort> cnsIRGTp15.txt") 
      or die "cannot open IRGTp15.txt";

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
         $fd = $tagtofilemap{$tag . "-" . $pnum};
         $fd->print("0x" . $cvalue . "\t0x" . substr($u,2,4) . "\t# <CJK>\n");
    }
  }
}

