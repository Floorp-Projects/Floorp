#!/usr/bin/perl

#
# gre.pl
#

while (<STDIN>) {
  s/\[Embed\]/\[$ARGV[0]\]/;
  print;
}
