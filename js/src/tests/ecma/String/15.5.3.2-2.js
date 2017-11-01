/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.3.2-2.js
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

   This tests String.fromCharCode with multiple arguments.

   Author:             christine@netscape.com
   Date:               2 october 1997
*/

var SECTION = "15.5.3.2-2";
var TITLE   = "String.fromCharCode()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "var MYSTRING = String.fromCharCode(eval(\"var args=''; for ( i = 0x0020; i < 0x007f; i++ ) { args += ( i == 0x007e ) ? i : i + ', '; } args;\")); MYSTRING",
	      " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
	      eval( "var MYSTRING = String.fromCharCode(" + eval("var args=''; for ( i = 0x0020; i < 0x007f; i++ ) { args += ( i == 0x007e ) ? i : i + ', '; } args;") +"); MYSTRING" ));

new TestCase( "MYSTRING.length",
	      0x007f - 0x0020,
	      MYSTRING.length );

test();
