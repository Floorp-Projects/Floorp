#!/usr/bin/perl

#
# mre.pl
#

while (<STDIN>) {
  s/\[Embed\]/\[$ARGV[0]\]/;
  print;
}
