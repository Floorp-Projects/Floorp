/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.2.js
   ECMA Section:       15.5.2 The String Constructor
   15.5.2.1 new String(value)
   15.5.2.2 new String()

   Description:	When String is called as part of a new expression, it
   is a constructor; it initializes the newly constructed
   object.

   - The prototype property of the newly constructed
   object is set to the original String prototype object,
   the one that is the intial value of String.prototype
   - The internal [[Class]] property of the object is "String"
   - The value of the object is ToString(value).
   - If no value is specified, its value is the empty string.

   Author:             christine@netscape.com
   Date:               1 october 1997
*/

var SECTION = "15.5.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The String Constructor";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( SECTION,	"typeof new String('string primitive')",	    "object",	        typeof new String('string primitive') );
new TestCase( SECTION,	"var TESTSTRING = new String('string primitive'); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()", "[object String]",   eval("var TESTSTRING = new String('string primitive'); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()") );
new TestCase( SECTION,  "(new String('string primitive')).valueOf()",   'string primitive', (new String('string primitive')).valueOf() );
new TestCase( SECTION,  "(new String('string primitive')).substring",   String.prototype.substring,   (new String('string primitive')).substring );

new TestCase( SECTION,	"typeof new String(void 0)",	                "object",	        typeof new String(void 0) );
new TestCase( SECTION,	"var TESTSTRING = new String(void 0); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()", "[object String]",   eval("var TESTSTRING = new String(void 0); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()") );
new TestCase( SECTION,  "(new String(void 0)).valueOf()",               "undefined", (new String(void 0)).valueOf() );
new TestCase( SECTION,  "(new String(void 0)).toString",               String.prototype.toString,   (new String(void 0)).toString );

new TestCase( SECTION,	"typeof new String(null)",	            "object",	        typeof new String(null) );
new TestCase( SECTION,	"var TESTSTRING = new String(null); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()", "[object String]",   eval("var TESTSTRING = new String(null); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()") );
new TestCase( SECTION,  "(new String(null)).valueOf()",         "null",             (new String(null)).valueOf() );
new TestCase( SECTION,  "(new String(null)).valueOf",         String.prototype.valueOf,   (new String(null)).valueOf );

new TestCase( SECTION,	"typeof new String(true)",	            "object",	        typeof new String(true) );
new TestCase( SECTION,	"var TESTSTRING = new String(true); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()", "[object String]",   eval("var TESTSTRING = new String(true); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()") );
new TestCase( SECTION,  "(new String(true)).valueOf()",         "true",             (new String(true)).valueOf() );
new TestCase( SECTION,  "(new String(true)).charAt",         String.prototype.charAt,   (new String(true)).charAt );

new TestCase( SECTION,	"typeof new String(false)",	            "object",	        typeof new String(false) );
new TestCase( SECTION,	"var TESTSTRING = new String(false); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()", "[object String]",   eval("var TESTSTRING = new String(false); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()") );
new TestCase( SECTION,  "(new String(false)).valueOf()",        "false",            (new String(false)).valueOf() );
new TestCase( SECTION,  "(new String(false)).charCodeAt",        String.prototype.charCodeAt,   (new String(false)).charCodeAt );

new TestCase( SECTION,	"typeof new String(new Boolean(true))",	       "object",	        typeof new String(new Boolean(true)) );
new TestCase( SECTION,	"var TESTSTRING = new String(new Boolean(true)); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()", "[object String]",   eval("var TESTSTRING = new String(new Boolean(true)); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()") );
new TestCase( SECTION,  "(new String(new Boolean(true))).valueOf()",   "true",              (new String(new Boolean(true))).valueOf() );
new TestCase( SECTION,  "(new String(new Boolean(true))).indexOf",   String.prototype.indexOf,    (new String(new Boolean(true))).indexOf );

new TestCase( SECTION,	"typeof new String()",	                        "object",	        typeof new String() );
new TestCase( SECTION,	"var TESTSTRING = new String(); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()", "[object String]",   eval("var TESTSTRING = new String(); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()") );
new TestCase( SECTION,  "(new String()).valueOf()",   '',                 (new String()).valueOf() );
new TestCase( SECTION,  "(new String()).lastIndexOf",   String.prototype.lastIndexOf,   (new String()).lastIndexOf );

new TestCase( SECTION,	"typeof new String('')",	    "object",	        typeof new String('') );
new TestCase( SECTION,	"var TESTSTRING = new String(''); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()", "[object String]",   eval("var TESTSTRING = new String(''); TESTSTRING.toString=Object.prototype.toString;TESTSTRING.toString()") );
new TestCase( SECTION,  "(new String('')).valueOf()",   '',                 (new String('')).valueOf() );
new TestCase( SECTION,  "(new String('')).split",   String.prototype.split,   (new String('')).split );

test();
