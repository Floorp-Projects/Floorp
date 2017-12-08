/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.3-2.js
   ECMA Section:       15.5.4.3 String.prototype.valueOf()

   Description:        Returns this string value.

   The valueOf function is not generic; it generates a
   runtime error if its this value is not a String object.
   Therefore it connot be transferred to the other kinds of
   objects for use as a method.

   Author:             christine@netscape.com
   Date:               1 october 1997
*/


var SECTION = "15.5.4.3-2";
var TITLE   = "String.prototype.valueOf";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "var valof=String.prototype.valueOf; astring=new String(); astring.valueOf = valof; astring.valof()",
	      "",
	      eval("var valof=String.prototype.valueOf; astring=new String(); astring.valueOf = valof; astring.valueOf()") );

new TestCase( "var valof=String.prototype.valueOf; astring=new String(0); astring.valueOf = valof; astring.valof()",
	      "0",
	      eval("var valof=String.prototype.valueOf; astring=new String(0); astring.valueOf = valof; astring.valueOf()") );

new TestCase( "var valof=String.prototype.valueOf; astring=new String('hello'); astring.valueOf = valof; astring.valof()",
	      "hello",
	      eval("var valof=String.prototype.valueOf; astring=new String('hello'); astring.valueOf = valof; astring.valueOf()") );

new TestCase( "var valof=String.prototype.valueOf; astring=new String(''); astring.valueOf = valof; astring.valof()",
	      "",
	      eval("var valof=String.prototype.valueOf; astring=new String(''); astring.valueOf = valof; astring.valueOf()") );
/*
  new TestCase( "var valof=String.prototype.valueOf; astring=new Number(); astring.valueOf = valof; astring.valof()",
  "error",
  eval("var valof=String.prototype.valueOf; astring=new Number(); astring.valueOf = valof; astring.valueOf()") );
*/

test();
