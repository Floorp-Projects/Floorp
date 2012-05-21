/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.3-3-n.js
   ECMA Section:       15.5.4.3 String.prototype.valueOf()

   Description:        Returns this string value.

   The valueOf function is not generic; it generates a
   runtime error if its this value is not a String object.
   Therefore it connot be transferred to the other kinds of
   objects for use as a method.

   Author:             christine@netscape.com
   Date:               1 october 1997
*/


var SECTION = "15.5.4.3-3-n";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.prototype.valueOf";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "var valof=String.prototype.valueOf; astring=new Number(); astring.valueOf = valof; astring.valof()";
EXPECTED = "error";

new TestCase( SECTION,
	      "var valof=String.prototype.valueOf; astring=new Number(); astring.valueOf = valof; astring.valof()",
	      "error",
	      eval("var valof=String.prototype.valueOf; astring=new Number(); astring.valueOf = valof; astring.valueOf()") );

test();
