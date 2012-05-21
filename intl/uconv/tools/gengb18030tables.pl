#!/usr/local/bin/perl
# -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
%gb18030tounicode = {};
%unicodetogb18030 = {};
%unicodetocp936 = {};
%cp936tounicode = {};
%tounicodecommon = {};
%gb18030tounicodeuniq = {};
%gb180304btounicode = {};
%cp936tounicodeuniq = {};

%map = {};
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
sub addeudc()
{
  my($l,$h,$hl,$us);

  $u = 0xE000;
  $us = sprintf "%04X", $u;
  # For AAA1-AFFE
  for($h=0xAA; $h <=0xAF;$h++)
  {
    for($l=0xA1; $l <=0xFE;$l++,$u++)
    {
        $us = sprintf "%04X", $u;
        $hl = sprintf "%02X%02X", $h, $l;
        $unicodetocp936{$us} = $hl;
    }
  }

  # For F8A1-FEFE
  $us = sprintf "%04X", $u;
  for($h=0xF8; $h <=0xFE;$h++)
  {
    for($l=0xA1; $l <=0xFE;$l++,$u++)
    {
        $us = sprintf "%04X", $u;
        $hl = sprintf "%02X%02X", $h, $l;
        $unicodetocp936{$us} = $hl;
    }
  }

  # For A140-A7A0
  $us = sprintf "%04X", $u;
  for($h=0xA1; $h <=0xA7;$h++)
  {
    for($l=0x40; $l <=0x7E;$l++,$u++)
    {
        $us = sprintf "%04X", $u;
        $hl = sprintf "%02X%02X", $h, $l;
        $unicodetocp936{$us} = $hl;
    }
    # We need to skip 7F
    for($l=0x80; $l <=0xA0;$l++,$u++)
    {
        $us = sprintf "%04X", $u;
        $hl = sprintf "%02X%02X", $h, $l;
        $unicodetocp936{$us} = $hl;
    }
  }
}

sub readcp936()
{
  open(CP936, "<CP936.txt") || die "Cannot open CP936 file";
  while(<CP936>)
  {
    if(! /^#/) {
      chop();
      ($gb, $u) = split(/\t/, $_);
      if($u =~ /^0x/) {
        $u1 = substr($u, 2, 4);
        $gb1 = substr($gb, 2, 4);
        $cp936tounicode{$gb1} = $u1;
        if($unicodetocp936{$u1} == "") {
          $unicodetocp936{$u1} = $gb1;
        } else {
          "WARNING: Unicode " . $u1 . " already map to CP936 " . 
            $unicodetocp936{$u1} . " when we try to map to " . $gb1 . "\n";
        }

      }
    }
  }
}
sub readgb18030()
{
  open(GB18030, "<GB18030") || die "Cannot open GB18030 file";
  while(<GB18030>)
  {
    if(/^[0-9A-F]/) {
      chop();
      ($u, $gb) = split(/\s/, $_);
      $gb18030tounicode{$gb} = $u;
        if( $unicodetogb18030{$u} == "" ) {
          $unicodetogb18030{$u} = $gb;
        } else {
          "WARNING: Unicode " . $u1 . " already map to CP936 " . 
            $unicodetocp936{$u1} . " when we try to map to " . $gb1 . "\n";
        }
    }
  }
}
sub splittable()
{
  my($i, $u);
  for($i = 0; $i < 0x10000; $i++) {
     $u = sprintf "%04X", $i;
     if($unicodetogb18030{$u} eq $unicodetocp936{$u}) {
        if($unicodetogb18030{$u} ne "") {
          $tounicodecommon{$unicodetogb18030{$u}} = $u;
        } else {
#          print $u . "|" . $unicodetogb18030{$u} . "|" . $unicodetocp936{$u} . "\n";
        }
     } else {
        if($unicodetogb18030{$u} ne "" ) {
           if($unicodetogb18030{$u}.length > 4) {
             $gb180304btounicode{$unicodetogb18030{$u}} = $u;
           } else {
             $gb18030tounicodeuniq{$unicodetogb18030{$u}} = $u;
           }
        } 
        if($unicodetocp936{$u} ne "" ) {
           $cp936tounicodeuniq{$unicodetocp936{$u}} = $u;
        }
     }
  }
}
sub gb4bytestoidx()
{
  my($gb) = @_;
  my($b1,$b2, $b3, $b4,$idx);
  $b1 = hex(substr($gb, 0, 2)) - 0x81;
  $b2 = hex(substr($gb, 2, 2)) - 0x30;
  $b3 = hex(substr($gb, 4, 2)) - 0x81;
  $b4 = hex(substr($gb, 6, 2)) - 0x30;
  $idx = sprintf "%04X" , ((($b1 * 10) + $b2 ) * 126 + $b3) * 10 + $b4;
  return $idx;
}
sub printcommontable()
{
  open ( GBKCOMMON, ">gbkcommon.txt" ) || die "cannot open gbkcommon.txt";
  foreach $gb (sort(keys %tounicodecommon)) {
      print GBKCOMMON "0x" . $gb . "\t0x" . $tounicodecommon{$gb} . "\n";
  }
  close GBKCOMMON;
}
sub printcp936table()
{
  open ( CP936UNIQ, ">cp936uniq.txt" ) || die "cannot open cp936uniq.txt";
  foreach $gb (sort(keys %cp936tounicodeuniq)) {
      print CP936UNIQ "0x" . $gb . "\t0x" . $cp936tounicodeuniq{$gb} . "\n";
  }
  close CP936UNIQ;
}
sub printgb180304btable()
{
  open ( GB180304B, ">gb180304b.txt" ) || die "cannot open gb180304b.txt";
  foreach $gb (sort(keys %gb180304btounicode)) {
      if($gb180304btounicode{$gb} ne "FFFF" ) {
        print GB180304B "0x" . &gb4bytestoidx($gb) . "\t0x" . $gb180304btounicode{$gb} . "\t# 0x" . $gb . "\n";
      }
  }
  close GB180304B;
}
sub printgb18030table()
{
  open ( GB18030UNIQ, ">gb18030uniq.txt" ) || die "cannot open gb18030uniq.txt";
  foreach $gb (sort(keys %gb18030tounicodeuniq)) {
      print GB18030UNIQ "0x" . $gb . "\t0x" . $gb18030tounicodeuniq{$gb} . "\n";
  }
  close GB18030UNIQ;
}

sub genufut()
{
 print ( "umaptable -uf < gb18030uniq.txt > gb18030uniq2b.uf\n");
 system( "umaptable -uf < gb18030uniq.txt > gb18030uniq2b.uf");

 print ( "umaptable -ut < gb18030uniq.txt > gb18030uniq2b.ut\n");
 system( "umaptable -ut < gb18030uniq.txt > gb18030uniq2b.ut");

 print ( "umaptable -uf < cp936uniq.txt > gbkuniq2b.uf\n") ;
 system( "umaptable -uf < cp936uniq.txt > gbkuniq2b.uf") ;

 print ( "umaptable -ut < cp936uniq.txt > gbkuniq2b.ut\n") ;
 system( "umaptable -ut < cp936uniq.txt > gbkuniq2b.ut") ;

 print ( "umaptable -uf < gb180304b.txt > gb180304bytes.uf\n")  ;
 system( "umaptable -uf < gb180304b.txt > gb180304bytes.uf")  ;

 print ( "umaptable -ut < gb180304b.txt > gb180304bytes.ut\n")  ;
 system( "umaptable -ut < gb180304b.txt > gb180304bytes.ut")  ;

 print ( "perl cp936tocdx.pl > cp936map.h\n");
 system( "perl cp936tocdx.pl > cp936map.h");
}

&readgb18030();
&readcp936();
&addeudc();
&splittable();
&printcommontable();
&printgb180304btable();
&printgb18030table();
&printcp936table();
&genufut();
