#!/usr/local/bin/perl
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
# IBM Corporation.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

######################################################################
#
# Initial global variable
#
######################################################################

%gcount = ();
%pat = ();

%map = (
  "L"   =>  "1", # Left-to-Right			  	
  "R"   =>  "2", # Right-to-Left			  
  "AL"  =>  "3", # Right-to-Left Arabic		  
  "AN"  =>  "4", # Arabic Number			  
  "EN"  =>  "5", # European Number			  	
  "ES"  =>  "6", # European Number Separator  
  "ET"  =>  "7", # European Number Terminator 	
  "CS"  =>  "8", # Common Number Separator	  
  "ON"  =>  "9", # Other Neutrals             
  "NSM" => "10", # Non-Spacing Mark			  
  "BN"  => "11", # Boundary Neutral			  
  "B"   => "12", # Paragraph Separator		  
  "S"   => "13", # Segment Separator		  
  "WS"  => "14", # Whitespace				  
  "LRE" => "15", # Left-to-Right Embedding	  
  "RLE" => "15", # Right-to-Left Embedding	  
  "PDF" => "15", # Pop Directional Format	  
  "LRO" => "15", # Left-to-Right Override	  	
  "RLO" => "15"  # Right-to-Left Override	  	
);

%special = ();

######################################################################
#
# Open the unicode database file
#
######################################################################
open ( UNICODATA , "< UnicodeData-Latest.txt") 
   || die "cannot find UnicodeData-Latest.txt";

######################################################################
#
# Open the output file
#
######################################################################
open ( OUT , "> ../base/src/bidicattable.h") 
  || die "cannot open output ../base/src/bidicattable.h file";

######################################################################
#
# Generate license and header
#
######################################################################
$npl = <<END_OF_NPL;
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
/* 
    DO NOT EDIT THIS DOCUMENT !!! THIS DOCUMENT IS GENERATED BY
    mozilla/layout/tools/genbidicattable.pl
 */
END_OF_NPL
print OUT $npl;
print OUT "\n\n#include \"nscore.h\" \n\n";


%bidicategory = ();
%sh = ();
%sl = ();
%sc = ();

######################################################################
#
# Process the file line by line
#
######################################################################
while(<UNICODATA>) {
   chop;
   ######################################################################
   #
   # Get value from fields
   #
   ######################################################################
   @f = split(/;/ , $_); 
   $c = $f[0];   # The unicode value
   $n = $f[1];   # The unicode name
   $g = $f[2];   # The General Category 
   $b = $f[4];   # The Bidi Category 

   if(( substr($n, 0, 1) ne "<")  || ($n eq "<control>"))
   {
      #
      # print $g;
      # 
     
      $gcount{$b}++;
      $bidicategory{$c} = $b;
   } else {

      # Handle special block
      @pair=split(/, /,  $n );
      $catnum = $map{$b};

      # printf "[%s][%s] => %d\n", $pair[0], $pair[1], $catnum;
      if( $pair[1] eq "First>") {
         $sl{$pair[0]} = $c;
         $sc{$pair[0]} = $catnum;
      } elsif ( $pair[1] eq "Last>") {
         $sh{$pair[0]} = $c;
         if($sc{$pair[0]} ne $catnum)
         {
            print "WARNING !!!! error in handling special block\n\n";
         }
      } else {
         print "WARNING !!!! error in handling special block\n\n";
      }
   }
}

# XXX - How can this be made more flexible as new blocks are added to the UCDB?

@range = (
  0x0000,   0x07ff, 
  0x0900,   0x19ff,
  0x1d00,   0x2bff,
  0x2e80,   0x33ff,
  0x4dc0,   0x4dff,
  0xa000,   0xa4ff,	  
  0xf900,  0x1013f,
  0x10300, 0x104ff,
  0x10800, 0x1083f,
  0x1d000, 0x1d1ff,
  0x1d300, 0x1d7ff,
  0x2f800, 0x2fa1f,
  0xe0000, 0xe01ff  
);


$totaldata = 0;

$tt=($#range+1) / 2;
@patarray = ();


# This should improve performance: put all the patterns like 0x11111111, 0x22222222 etc at the beginning of the table.
#  Since there are a lot of blocks with the same category, we should be able to save a lot of time extracting the digits
for (0..15) {
	$pattern = "0x".(sprintf("%X", $_) x 8);
	$patarray[$_] = $pattern;
	$pat{$pattern} = $_;
}

$newidx = 0x10;

for($t = 1; $t <= $tt; $t++)
{
	$tl = $range[($t-1) * 2];
	$th = $range[($t-1) * 2 + 1];
	$ts = ( $th - $tl ) >> 3;
	$totaldata += $ts + 1;
	printf OUT "static PRUint8 gBidiCatIdx%d[%d] = {\n", $t, $ts + 1;
	for($i = ($tl >> 3); $i <= ($th >> 3) ; $i ++ )
	{
      $data = 0;
		      
		for($j = 0; $j < 8 ; $j++)
		{
			#defaults for unassigned characters
		        #see http://www.unicode.org/Public/UNIDATA/UCD.html#Bidi_Class
			$test = ($i << 3) + $j;
			if ((($test >= 0x0590) && ($test <= 0x5FF)) ||
			    (($test >= 0x07C0) && ($test <= 0x8FF)) ||
			    (($test >= 0xFB1D) && ($test <= 0xFB4F)) ||
			    (($test >= 0x10800) && ($test <=0x10FFF)))
			{
				$default = $map{"R"};
			} elsif ((($test >= 0x0600) && ($test <= 0x7BF)) ||
				 (($test >= 0xFB50) && ($test <= 0xFDCF)) ||
				 (($test >= 0xFDF0) && ($test <= 0xFDFF)) ||
				 (($test >= 0xFE70) && ($test <= 0xFEFE)))
			{
				$default = $map{"AL"};
			} else
			{
				$default = $map{"L"};
			}
			$k =  sprintf("%04X", (($i << 3) + $j));
			
			$cat =  $bidicategory{$k};
			if( $cat eq "")
			{
				$data = $data +  ($default << (4*$j));
			} else {
				$data = $data +  ($map{$cat} << (4*$j));
			}
			
		}
		$pattern = sprintf("0x%08X", $data);
   
		$idx = $pat{$pattern};
		unless( exists($pat{$pattern})){
			$idx = $newidx++;
			$patarray[$idx] = $pattern;
			$pat{$pattern} = $idx;
		}

      printf OUT "    %3d,  /* U+%04X - U+%04X : %s */\n" , 
                 $idx, ($i << 3),((($i +1)<< 3)-1), $pattern ;

   
   }
   printf OUT "};\n\n";

   if($t ne $tt)
   {
       $tl = $range[($t-1) * 2 + 1] + 1;
       $th = $range[$t * 2] - 1;
       for($i = ($tl >> 3); $i <= ($th >> 3) ; $i ++ )
       {
          $data = 0;
          for($j = 0; $j < 8 ; $j++)
          {
             $k =  sprintf("%04X", (($i << 3) + $j));
      
             $cat =  $bidicategory{$k};
             if( $cat ne "")
             {
                 $data = $data +  ($map{$cat} << (4*$j));
             }
          }
          $pattern = sprintf("0x%08X", $data);
          if($data ne 0)
          {
             print "WARNING, Unicode Database now contain characters" .
                   "which we have not consider, change this program !!!\n\n";
             printf "Problem- U+%04X - U+%04X range\n", ($i << 3),((($i +1)<< 3)-1);
          }
       }
   }
}


if($newidx > 255)
{
  die "We have more than 255 patterns !!! - $newidx\n\n" .
      "This program is now broken!!!\n\n\n";      

}
printf OUT "static PRUint32 gBidiCatPat[$newidx] = {\n";
for($i = 0 ; $i < $newidx; $i++)
{
   printf OUT "    %s,  /* $i */\n", $patarray[$i] ;
}
printf OUT "};\n\n";
$totaldata += $newidx * 4;

printf OUT "static eBidiCategory GetBidiCat(PRUint32 u)\n{\n";
printf OUT "    PRUint32 pat;\n";
printf OUT "    PRUint16 patidx;\n\n";

@special = keys(%sh);
$sp = 0;
foreach $s ( sort(@special) ) {
	# don't bother to define the special blocks unless they have a different
    #  value from the default they would be given if they were undefined
	unless ($sc{$s} == $map{"L"}) {
		unless ($sp++) {
			%by_value = reverse %map;
			printf OUT "    /*  Handle blocks which share the same category */\n\n";
		}
		printf OUT "    /* Handle %s block */\n", substr($s, 1);
		printf OUT "    if((((PRUint32)0x%s)<=u)&&(u<=((PRUint32)0x%s))) \n", $sl{$s}, $sh{$s};
		printf OUT "        return eBidiCat_$by_value{$sc{$s}}; \n\n";
	}
}

printf OUT "    /*  Handle blocks which use index table mapping */   \n\n";
for($t = 1; $t <= $tt; $t++)
{
   $tl = $range[($t-1) * 2];
   $th = $range[($t-1) * 2 + 1];
   if ($tl == 0) {
	   printf OUT "    /* Handle U+%04X to U+%04X */\n", $tl, $th;
	   printf OUT "    if (u<=((PRUint32)0x%04X)) {\n", $th;
	   printf OUT "        patidx = gBidiCatIdx%d [( u  >> 3 )];\n", $t;
   } else {
	   printf OUT "    /* Handle U+%04X to U+%04X */\n", $tl, $th;
	   printf OUT "    else if ((((PRUint32)0x%04X)<=u)&&(u<=((PRUint32)0x%04X))) {\n", $tl, $th;
	   printf OUT "        patidx = gBidiCatIdx%d [( (u -(PRUint32) 0x%04X) >> 3 )];\n", $t, $tl;
   }
   printf OUT "    }\n\n";
}
printf OUT "    else {\n";
printf OUT "        return eBidiCat_L; /* UNDEFINE = L */\n";
printf OUT "    }\n\n";

printf OUT "    if (patidx < 0x10)\n";
printf OUT "        return (eBidiCategory)patidx;\n";
printf OUT "    else {\n";
printf OUT "        pat = gBidiCatPat[patidx];\n";
printf OUT "        return (eBidiCategory)((pat  >> ((u % 8) * 4)) & 0x0F);\n";
printf OUT "    }\n}\n\n";

printf OUT "/* total data size = $totaldata */\n";
print "total = $totaldata\n";

######################################################################
#
# Close files
#
######################################################################
close(UNIDATA);
close(OUT);

