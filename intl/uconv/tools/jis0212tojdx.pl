#!/user/local/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

sub jis0212tonum()
{
   my($jis0212) = (@_);
   my($first,$second,$jnum);
   $first = hex(substr($jis0212,2,2));
   $second = hex(substr($jis0212,4,2));
   $jnum = (($first - 0x21) * 94);
   $jnum += $second - 0x21 ;
   return $jnum;
}

@map = {};
sub readtable()
{
open(JIS0212, "<JIS0212") || die "cannot open JIS0212";
while(<JIS0212>)
{
   if(! /^#/) {
        ($j, $u, $r) = split(/\t/,$_);
        if(length($j) > 4)
        {
        $n = &jis0212tonum($j);
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

