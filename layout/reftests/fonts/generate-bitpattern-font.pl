#!/usr/bin/perl -w

# Generates an SVG Font where each glyph (identified on stdin by four
# hex characters) consists of a bit pattern representing the Unicode
# code point it is the glyph for.

use strict;

print <<EOF;
<svg xmlns="http://www.w3.org/2000/svg">
<font id="BitPattern" horiz-adv-x="1000">
  <font-face font-family="BitPattern" units-per-em="1000" ascent="800"/>
EOF

while (<>) {
  chomp;
  next if /^\s*$/;
  die unless /^[0-9A-Fa-f]{4}$/;
  my $c = hex;
  my $s = "  <glyph unicode='&#x$_;' d='";
  for (my $i = 0; $i < 32; $i++) {
    if ($c & (1 << $i)) {
      my $x = 100 * (7 - ($i % 8));
      my $y = 100 * int($i / 8);
      $s .= "M$x,$y v80h80v-80z ";
    }
  }
  $s .= "'/>\n";
  print $s;
}

print <<EOF;
</font>
</svg>
EOF
