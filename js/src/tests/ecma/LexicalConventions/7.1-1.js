/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.1-1.js
   ECMA Section:       7.1 White Space
   Description:        - readability
   - separate tokens
   - otherwise should be insignificant
   - in strings, white space characters are significant
   - cannot appear within any other kind of token

   white space characters are:
   unicode     name            formal name     string representation
   \u0009      tab             <TAB>           \t
   \u000B      veritical tab   <VT>            \v
   \U000C      form feed       <FF>            \f
   \u0020      space           <SP>            " "

   Author:             christine@netscape.com
   Date:               11 september 1997
*/

var SECTION = "7.1-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "White Space";

writeHeaderToLog( SECTION + " "+ TITLE);

// whitespace between var keyword and identifier

new TestCase( SECTION,  'var'+'\t'+'MYVAR1=10;MYVAR1',   10, eval('var'+'\t'+'MYVAR1=10;MYVAR1') );
new TestCase( SECTION,  'var'+'\f'+'MYVAR2=10;MYVAR2',   10, eval('var'+'\f'+'MYVAR2=10;MYVAR2') );
new TestCase( SECTION,  'var'+'\v'+'MYVAR2=10;MYVAR2',   10, eval('var'+'\v'+'MYVAR2=10;MYVAR2') );
new TestCase( SECTION,  'var'+'\ '+'MYVAR2=10;MYVAR2',   10, eval('var'+'\ '+'MYVAR2=10;MYVAR2') );

// use whitespace between tokens object name, dot operator, and object property

new TestCase( SECTION,
	      "var a = new Array(12345); a\t\v\f .\\u0009\\000B\\u000C\\u0020length",
	      12345,
	      eval("var a = new Array(12345); a\t\v\f .\u0009\u0020\u000C\u000Blength") );

test();
