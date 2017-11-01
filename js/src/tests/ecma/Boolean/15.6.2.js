/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.2.js
   ECMA Section:       15.6.2 The Boolean Constructor
   15.6.2.1 new Boolean( value )
   15.6.2.2 new Boolean()

   This test verifies that the Boolean constructor
   initializes a new object (typeof should return
   "object").  The prototype of the new object should
   be Boolean.prototype.  The value of the object
   should be ToBoolean( value ) (a boolean value).

   Description:
   Author:             christine@netscape.com
   Date:               june 27, 1997

*/
var SECTION = "15.6.2";
var TITLE   = "15.6.2 The Boolean Constructor; 15.6.2.1 new Boolean( value ); 15.6.2.2 new Boolean()";

writeHeaderToLog( SECTION + " "+ TITLE);

var array = new Array();
var item = 0;

new TestCase( "typeof (new Boolean(1))",         "object",            typeof (new Boolean(1)) );
new TestCase( "(new Boolean(1)).constructor",    Boolean.prototype.constructor,   (new Boolean(1)).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(1);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(1);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(1)).valueOf()",   true,       (new Boolean(1)).valueOf() );
new TestCase( "typeof new Boolean(1)",         "object",   typeof new Boolean(1) );
new TestCase( "(new Boolean(0)).constructor",    Boolean.prototype.constructor,   (new Boolean(0)).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(0);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(0);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(0)).valueOf()",   false,       (new Boolean(0)).valueOf() );
new TestCase( "typeof new Boolean(0)",         "object",   typeof new Boolean(0) );
new TestCase( "(new Boolean(-1)).constructor",    Boolean.prototype.constructor,   (new Boolean(-1)).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(-1);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(-1);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(-1)).valueOf()",   true,       (new Boolean(-1)).valueOf() );
new TestCase( "typeof new Boolean(-1)",         "object",   typeof new Boolean(-1) );
new TestCase( "(new Boolean('1')).constructor",    Boolean.prototype.constructor,   (new Boolean('1')).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean('1');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean('1');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean('1')).valueOf()",   true,       (new Boolean('1')).valueOf() );
new TestCase( "typeof new Boolean('1')",         "object",   typeof new Boolean('1') );
new TestCase( "(new Boolean('0')).constructor",    Boolean.prototype.constructor,   (new Boolean('0')).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean('0');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean('0');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean('0')).valueOf()",   true,       (new Boolean('0')).valueOf() );
new TestCase( "typeof new Boolean('0')",         "object",   typeof new Boolean('0') );
new TestCase( "(new Boolean('-1')).constructor",    Boolean.prototype.constructor,   (new Boolean('-1')).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean('-1');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean('-1');TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean('-1')).valueOf()",   true,       (new Boolean('-1')).valueOf() );
new TestCase( "typeof new Boolean('-1')",         "object",   typeof new Boolean('-1') );
new TestCase( "(new Boolean(new Boolean(true))).constructor",    Boolean.prototype.constructor,   (new Boolean(new Boolean(true))).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(new Boolean(true));TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(new Boolean(true));TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(new Boolean(true))).valueOf()",   true,       (new Boolean(new Boolean(true))).valueOf() );
new TestCase( "typeof new Boolean(new Boolean(true))",         "object",   typeof new Boolean(new Boolean(true)) );
new TestCase( "(new Boolean(Number.NaN)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NaN)).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(Number.NaN);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(Number.NaN);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(Number.NaN)).valueOf()",   false,       (new Boolean(Number.NaN)).valueOf() );
new TestCase( "typeof new Boolean(Number.NaN)",         "object",   typeof new Boolean(Number.NaN) );
new TestCase( "(new Boolean(null)).constructor",    Boolean.prototype.constructor,   (new Boolean(null)).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(null);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(null);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(null)).valueOf()",   false,       (new Boolean(null)).valueOf() );
new TestCase( "typeof new Boolean(null)",         "object",   typeof new Boolean(null) );
new TestCase( "(new Boolean(void 0)).constructor",    Boolean.prototype.constructor,   (new Boolean(void 0)).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(void 0);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(void 0);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(void 0)).valueOf()",   false,       (new Boolean(void 0)).valueOf() );
new TestCase( "typeof new Boolean(void 0)",         "object",   typeof new Boolean(void 0) );
new TestCase( "(new Boolean(Number.POSITIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.POSITIVE_INFINITY)).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(Number.POSITIVE_INFINITY);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(Number.POSITIVE_INFINITY);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(Number.POSITIVE_INFINITY)).valueOf()",   true,       (new Boolean(Number.POSITIVE_INFINITY)).valueOf() );
new TestCase( "typeof new Boolean(Number.POSITIVE_INFINITY)",         "object",   typeof new Boolean(Number.POSITIVE_INFINITY) );
new TestCase( "(new Boolean(Number.NEGATIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NEGATIVE_INFINITY)).constructor );
new TestCase( 
	      "TESTBOOL=new Boolean(Number.NEGATIVE_INFINITY);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean(Number.NEGATIVE_INFINITY);TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean(Number.NEGATIVE_INFINITY)).valueOf()",   true,       (new Boolean(Number.NEGATIVE_INFINITY)).valueOf() );
new TestCase( "typeof new Boolean(Number.NEGATIVE_INFINITY)",         "object",   typeof new Boolean(Number.NEGATIVE_INFINITY) );
new TestCase( "(new Boolean(Number.NEGATIVE_INFINITY)).constructor",    Boolean.prototype.constructor,   (new Boolean(Number.NEGATIVE_INFINITY)).constructor );
new TestCase( "TESTBOOL=new Boolean();TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()",
	      "[object Boolean]",
	      eval("TESTBOOL=new Boolean();TESTBOOL.toString=Object.prototype.toString;TESTBOOL.toString()") );
new TestCase( "(new Boolean()).valueOf()",   false,       (new Boolean()).valueOf() );
new TestCase( "typeof new Boolean()",        "object",    typeof new Boolean() );

test();
