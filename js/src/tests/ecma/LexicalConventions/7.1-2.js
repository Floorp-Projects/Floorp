/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.1-2.js
   ECMA Section:       7.1 White Space
   Description:        - readability
   - separate tokens
   - otherwise should be insignificant
   - in strings, white space characters are significant
   - cannot appear within any other kind of token

   white space characters are:
   unicode     name            formal name     string representation
   \u0009      tab             <TAB>           \t
   \u000B      veritical tab   <VT>            ??
   \U000C      form feed       <FF>            \f
   \u0020      space           <SP>            " "

   Author:             christine@netscape.com
   Date:               11 september 1997
*/

var SECTION = "7.1-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "White Space";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,    "'var'+'\u000B'+'MYVAR1=10;MYVAR1'",   10, eval('var'+'\u000B'+'MYVAR1=10;MYVAR1') );
new TestCase( SECTION,    "'var'+'\u0009'+'MYVAR2=10;MYVAR2'",   10, eval('var'+'\u0009'+'MYVAR2=10;MYVAR2') );
new TestCase( SECTION,    "'var'+'\u000C'+'MYVAR3=10;MYVAR3'",   10, eval('var'+'\u000C'+'MYVAR3=10;MYVAR3') );
new TestCase( SECTION,    "'var'+'\u0020'+'MYVAR4=10;MYVAR4'",   10, eval('var'+'\u0020'+'MYVAR4=10;MYVAR4') );

test();
