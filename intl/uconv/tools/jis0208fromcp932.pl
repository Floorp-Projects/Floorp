#!/user/local/bin/perl
sub printsjisto0208()
{
   my($sjis) = (@_);
   my($first,$second,$h, $l, $j0208);
   $first = hex(substr($sjis,2,2));
   $second = hex(substr($sjis,4,2));
   $jnum=0;

   if($first < 0xE0)
   {
      $jnum = ($first - 0x81) * ((0xfd - 0x80)+(0x7f - 0x40));
   } else {
      $jnum = ($first - 0xe0 + (0xa0-0x81)) * ((0xfd - 0x80)+(0x7f - 0x40));
   }
   if($second >= 0x80)
   {
       $jnum += $second - 0x80 + (0x7f-0x40);
   }
   else
   {
       $jnum += $second - 0x40;
   }
   if(($jnum / 94 ) < 94) {
    printf "0x%02X%02X", (($jnum / 94) + 0x21), (($jnum % 94)+0x21);
   } else {
    printf "# 0x%02X%02X", (($jnum / 94) + 0x21), (($jnum % 94)+0x21);
   }
}

@map = {};
sub readtable()
{
open(CP932, "<CP932.TXT") || die "cannot open CP932.TXT";
while(<CP932>)
{
   if(! /^#/) {
        ($j, $u, $r) = split(/\t/,$_);
        if(length($j) > 4)
        {
        &printsjisto0208($j);
        print "\t" . $u . "\n";
        }
   } 
}
}

&readtable();
