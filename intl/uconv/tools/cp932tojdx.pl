#!/user/local/bin/perl
sub sjistonum()
{
   my($sjis) = (@_);
   my($first,$second,$jnum);
   $first = hex(substr($sjis,2,2));
   $second = hex(substr($sjis,4,2));
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
   return $jnum;
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
        $n = &sjistonum($j);
        $map{$n} = $u;
        }
   } 
}
}

## add eudc to  $map here

sub printtable()
{
for($i=0;$i<94;$i++)
{
     printf ( "/* 0x%2XXX */\n", ( $i + 0x21));
     printf "       ";
     for($j=0;$j<94;$j++)
     {
         if("" == ($map{($i * 94 + $j)}))
         {
            print "0xFFFD,"
         } 
         else 
         {   
            print $map{($i * 94 + $j)} . ",";
         }
         if( 7 == (($j + 1) % 8))
         {
            printf "/* 0x%2X%1X%1X*/\n", $i+0x21, 2+($j/16), (6==($j%16))?0:8;
         }
     }
     printf "       /* 0x%2X%1X%1X*/\n", $i+0x21, 2+($j/16),(6==($j%16))?0:8;
}
}
&readtable();
&printtable();

