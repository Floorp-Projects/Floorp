#!/usr/local/bin/perl
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.


# checkResources.pl
# Steve Lamm (slamm@netscape.com)
#
# Check the resources file for non-sanctioned colors
# (allowed colors are below)
#
# Usage:
#   checkResources.pl resources
#   checkResources.pl < resources

while (<DATA>) {
    ($rr, $gg, $bb) = /(\d+), +(\d+), +(\d+)/;
    $palette{"$rr,$gg,$bb"} = 1;
}

$hexColor = '([A-Fa-f0-9]{2})';

while (<>) {
    if (/^(.).*\#($hexColor$hexColor$hexColor)/) {
	$leadChar = $1;
	$rgb = $2;
	$rr = hex($3);
	$gg = hex($4);
	$bb = hex($5);

	$leadChar = " " if ($leadChar ne "!");

	if (!defined($palette{"$rr,$gg,$bb"})) {
	    ($tryrr, $trygg, $trybb) = getClosest($rr,$gg,$bb);

	    printf ("%s%5d:%s",$leadChar,$.,$_);
	    printf ("%s%5d: warning: Bad color %6s (%3d %3d %3d)"
		    ."  try  %02x%02x%02x (%3d %3d %3d)\n",
		    $leadChar, $., $rgb, $rr, $gg, $bb, 
		    $tryrr, $trygg, $trybb,
		    $tryrr, $trygg, $trybb);
	}
    }
}

# Find closest match based on luminance values
sub getClosest {
    local ($keyrr, $keygg, $keybb) = @_;

    local $matchDiff = 255001;
    local $matchColor = "-1,-1,-1";

    foreach $color (keys %palette) {

	local ($rr, $gg, $bb) = split (/,/,$color);
	local $diff = abs($keyrr - $rr) * 299
               	    + abs($keygg - $gg) * 587
		    + abs($keybb - $bb) * 114;

	if ($matchDiff > $diff) {
	    $matchDiff = $diff;
	    $matchColor = $color;
	}
    }
    return split (/,/,$matchColor);
}
__END__
  { 255, 255,   0,   0 },  /*  0 */
  { 255, 102,  51,   0 },  /*  1 */
  { 128, 128,   0,   0 },  /*  2 */
  {   0, 255,   0,   0 },  /*  3 */
  {   0, 128,   0,   0 },  /*  4 */
  {  66, 154, 167,   0 },  /*  5 */
  {   0, 128, 128,   0 },  /*  6 */
  {   0,  55,  60,   0 },  /*  7 */
  {   0, 255, 255,   0 },  /*  8 */
  {   0,   0, 255,   0 },  /*  9 */
  {   0,   0, 128,   0 },  /* 10 */
  { 153, 153, 255,   0 },  /* 11 */
   { 102, 102, 204,   0 },  /* 12 */
  {  51,  51, 102,   0 },  /* 13 */
  { 255,   0,   0,   0 },  /* 14 */
  { 128,   0,   0,   0 },  /* 15 */
  { 255, 102, 204,   0 },  /* 16 */
  { 153,   0, 102,   0 },  /* 17 */
  { 255, 255, 255,   0 },  /* 18 */
  { 192, 192, 192,   0 },  /* 19 */
  { 128, 128, 128,   0 },  /* 20 */
  {  34,  34,  34,   0 },  /* 21 */
  {   0,   0,   0,   0 },  /* 22 */
  {  26,  95, 103,   0 },  /* 23 */
