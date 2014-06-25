/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.3.2-1.js
   ECMA Section:       15.5.3.2  String.fromCharCode( char0, char1, ... )
   Description:        Return a string value containing as many characters
   as the number of arguments.  Each argument specifies
   one character of the resulting string, with the first
   argument specifying the first character, and so on,
   from left to right.  An argument is converted to a
   character by applying the operation ToUint16_t and
   regarding the resulting 16bit integeras the Unicode
   encoding of a character.  If no arguments are supplied,
   the result is the empty string.

   This test covers Basic Latin (range U+0020 - U+007F)

   Author:             christine@netscape.com
   Date:               2 october 1997
*/

var SECTION = "15.5.3.2-3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.fromCharCode()";

writeHeaderToLog( SECTION + " "+ TITLE);

for ( CHARCODE = 0; CHARCODE < 256; CHARCODE++ ) {
  new TestCase(   SECTION,
		  "(String.fromCharCode(" + CHARCODE +")).charCodeAt(0)",
		  ToUint16(CHARCODE),
		  (String.fromCharCode(CHARCODE)).charCodeAt(0)
    );
}
for ( CHARCODE = 256; CHARCODE < 65536; CHARCODE+=333 ) {
  new TestCase(   SECTION,
		  "(String.fromCharCode(" + CHARCODE +")).charCodeAt(0)",
		  ToUint16(CHARCODE),
		  (String.fromCharCode(CHARCODE)).charCodeAt(0)
    );
}
for ( CHARCODE = 65535; CHARCODE < 65538; CHARCODE++ ) {
  new TestCase(   SECTION,
		  "(String.fromCharCode(" + CHARCODE +")).charCodeAt(0)",
		  ToUint16(CHARCODE),
		  (String.fromCharCode(CHARCODE)).charCodeAt(0)
    );
}
for ( CHARCODE = Math.pow(2,32)-1; CHARCODE < Math.pow(2,32)+1; CHARCODE++ ) {
  new TestCase(   SECTION,
		  "(String.fromCharCode(" + CHARCODE +")).charCodeAt(0)",
		  ToUint16(CHARCODE),
		  (String.fromCharCode(CHARCODE)).charCodeAt(0)
    );
}
for ( CHARCODE = 0; CHARCODE > -65536; CHARCODE-=3333 ) {
  new TestCase(   SECTION,
		  "(String.fromCharCode(" + CHARCODE +")).charCodeAt(0)",
		  ToUint16(CHARCODE),
		  (String.fromCharCode(CHARCODE)).charCodeAt(0)
    );
}
new TestCase( SECTION, "(String.fromCharCode(65535)).charCodeAt(0)",    65535,  (String.fromCharCode(65535)).charCodeAt(0) );
new TestCase( SECTION, "(String.fromCharCode(65536)).charCodeAt(0)",    0,      (String.fromCharCode(65536)).charCodeAt(0) );
new TestCase( SECTION, "(String.fromCharCode(65537)).charCodeAt(0)",    1,      (String.fromCharCode(65537)).charCodeAt(0) );

test();

function ToUint16( num ) {
  num = Number( num );
  if ( isNaN( num ) || num == 0 || num == Number.POSITIVE_INFINITY || num == Number.NEGATIVE_INFINITY ) {
    return 0;
  }

  var sign = ( num < 0 ) ? -1 : 1;

  num = sign * Math.floor( Math.abs( num ) );
  num = num % Math.pow(2,16);
  num = ( num > -65536 && num < 0) ? 65536 + num : num;
  return num;
}

