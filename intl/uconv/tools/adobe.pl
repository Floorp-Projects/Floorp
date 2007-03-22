#!/usr/local/bin/perl -w
while(<STDIN>)
{
  if(/^#/)
  {
      print $_;
  }
  else 
  {
     ($a, $b, $c,$d) = /^(....)(.)(..)(.*)$/;
     print "0x" . $c . $b . "0x" . $a .  $d . "\n";
  }
}
