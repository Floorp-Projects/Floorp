#!/user/local/bin/perl
$rowwidth = ((0xff - 0x80)+(0x7f - 0x40));
sub cp936tonum()
{
   my($cp936) = (@_);
   my($first,$second,$jnum);
   $first = hex(substr($cp936,2,2));
   $second = hex(substr($cp936,4,2));
   $jnum = ($first - 0x81 ) * $rowwidth;
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
open(CP936, "<CP936.TXT") || die "cannot open CP936.TXT";
while(<CP936>)
{
   if(! /^#/) {
        ($j, $u, $r) = split(/\t/,$_);
        if(length($j) > 4)
        {
        $n = &cp936tonum($j);
        $map{$n} = $u;
        }
   } 
}
}

sub addeudc()
{
  my($l,$h,$hl);
  $u = 0xE000;
  for($h=0xF8; $h <=0xFE;$h++)
  {
    for($l=0xA0; $l <=0xFF;$l++,$u++)
    {
        $us = sprintf "0x%04X", $u;
        $hl = sprintf "0x%02X%02X", $h, $l;
        $map{&cp936tonum($hl)}= $us ;
    }
  }
  for($h=0xAA; $h <=0xAF;$h++)
  {
    for($l=0xA0; $l <=0xFF;$l++,$u++)
    {
        $us = sprintf "0x%04X", $u;
        $hl = sprintf "0x%02X%02X", $h, $l;
        $map{&cp936tonum($hl)}= $us ;
    }
  }
}

sub printtable()
{
for($i=0;$i<126;$i++)
{
     printf ( "/* 0x%2XXX */\n", ( $i + 0x81));
     for($j=0;$j<(0x7f-0x40);$j++)
     {
         if("" eq ($map{($i * $rowwidth + $j)}))
         {
            printf "0xFFFD,"
         } 
         else 
         {   
            printf $map{($i * $rowwidth + $j)} . ",";
         }
         if( 0 == (($j + 1) % 8))
         {
            printf "/* 0x%2X%1X%1X*/\n", $i+0x81, 4+($j/16), (7==($j%16))?0:8;
         }
     }
     
	 print "0xFFFF,";	#let 0xXX7F map to 0xFFFF

     printf "/* 0x%2X%1X%1X*/\n", $i+0x81, 4+($j/16),(7==($j%16))?0:8;
     for($j=0;$j < (0xff-0x80);$j++)
     {
         if("" eq ($map{($i * $rowwidth + $j + 0x3f)}))		# user defined chars map to 0xFFFD
         {

			if ( ( $i == 125 ) and ( $j == (0xff - 0x80 - 1 )))
			{
				printf "0xFFFD";							#has no ',' followed last item
			}
			else
			{
				printf "0xFFFD,";
			}
         } 
		 else
		 {
			if ( ( $i == 125 ) and ( $j == (0xff - 0x80 - 1 )))
			{
				printf $map{($i * $rowwidth + $j + 0x3f)};	#has no ',' followed last item
			}
			else
			{
				printf $map{($i * $rowwidth + $j + 0x3f)} . ",";
			}
		 }
		  	
         if( 0 == (($j + 1) % 8))
         {
            printf "/* 0x%2X%1X%1X*/\n", $i+0x81, 8+($j/16), (7==($j%16))?0:8;
         }
     }
     printf "       /* 0x%2X%1X%1X*/\n", $i+0x81, 8+($j/16),(7==($j%16))?0:8;
}
}

&readtable();
&addeudc();
&printtable();

