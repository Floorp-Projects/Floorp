/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          8.4.js
   ECMA Section:       The String type
   Description:

   The String type is the set of all finite ordered sequences of zero or more
   Unicode characters. Each character is regarded as occupying a position
   within the sequence. These positions are identified by nonnegative
   integers. The leftmost character (if any) is at position 0, the next
   character (if any) at position 1, and so on. The length of a string is the
   number of distinct positions within it. The empty string has length zero
   and therefore contains no characters.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "8.4";
var TITLE   = "The String type";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "var s = ''; s.length",
	      0,
	      eval("var s = ''; s.length") );

new TestCase( "var s = ''; s.charAt(0)",
	      "",
	      eval("var s = ''; s.charAt(0)") );


for ( var i = 0x0041, TEST_STRING = "", EXPECT_STRING = ""; i < 0x007B; i++ ) {
  TEST_STRING += ("\\u"+ DecimalToHexString( i ) );
  EXPECT_STRING += String.fromCharCode(i);
}

new TestCase( "var s = '" + TEST_STRING+ "'; s",
	      EXPECT_STRING,
	      eval("var s = '" + TEST_STRING+ "'; s") );

new TestCase( "var s = '" + TEST_STRING+ "'; s.length",
	      0x007B-0x0041,
	      eval("var s = '" + TEST_STRING+ "'; s.length") );

test();

function DecimalToHexString( n ) {
  n = Number( n );
  var h = "";

  for ( var i = 3; i >= 0; i-- ) {
    if ( n >= Math.pow(16, i) ){
      var t = Math.floor( n  / Math.pow(16, i));
      n -= t * Math.pow(16, i);
      if ( t >= 10 ) {
	if ( t == 10 ) {
	  h += "A";
	}
	if ( t == 11 ) {
	  h += "B";
	}
	if ( t == 12 ) {
	  h += "C";
	}
	if ( t == 13 ) {
	  h += "D";
	}
	if ( t == 14 ) {
	  h += "E";
	}
	if ( t == 15 ) {
	  h += "F";
	}
      } else {
	h += String( t );
      }
    } else {
      h += "0";
    }
  }

  return h;
}

