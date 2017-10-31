/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.2-1.js
   ECMA Section:       7.2 Line Terminators
   Description:        - readability
   - separate tokens
   - may occur between any two tokens
   - cannot occur within any token, not even a string
   - affect the process of automatic semicolon insertion.

   white space characters are:
   unicode     name            formal name     string representation
   \u000A      line feed       <LF>            \n
   \u000D      carriage return <CR>            \r

   Author:             christine@netscape.com
   Date:               11 september 1997
*/
var SECTION = "7.2-1";
var TITLE   = "Line Terminators";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( "var a\nb =  5; ab=10;ab;",     10,     eval("var a\nb =  5; ab=10;ab") );
new TestCase( "var a\nb =  5; ab=10;b;",      5,      eval("var a\nb =  5; ab=10;b") );
new TestCase( "var a\rb =  5; ab=10;ab;",     10,     eval("var a\rb =  5; ab=10;ab") );
new TestCase( "var a\rb =  5; ab=10;b;",      5,      eval("var a\rb =  5; ab=10;b") );
new TestCase( "var a\r\nb =  5; ab=10;ab;",     10,     eval("var a\r\nb =  5; ab=10;ab") );
new TestCase( "var a\r\nb =  5; ab=10;b;",      5,      eval("var a\r\nb =  5; ab=10;b") );

test();
