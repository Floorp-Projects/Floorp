/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.2.1.js
   ECMA Section:       15.3.2.1 The Function Constructor
   new Function(p1, p2, ..., pn, body )

   Description:
   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.3.2.1-2";
var TITLE   = "The Function Constructor";

writeHeaderToLog( SECTION + " "+ TITLE);


var myfunc1 = new Function("a","b","c", "return a+b+c" );
var myfunc2 = new Function("a, b, c",   "return a+b+c" );
var myfunc3 = new Function("a,b", "c",  "return a+b+c" );

myfunc1.toString = Object.prototype.toString;
myfunc2.toString = Object.prototype.toString;
myfunc3.toString = Object.prototype.toString;

new TestCase( "myfunc1 = new Function('a','b','c'); myfunc.toString = Object.prototype.toString; myfunc.toString()",
	      "[object Function]",
	      myfunc1.toString() );

new TestCase( "myfunc1.length",                            3,                      myfunc1.length );
new TestCase( "myfunc1.prototype.toString()",              "[object Object]",      myfunc1.prototype.toString() );

new TestCase( "myfunc1.prototype.constructor",             myfunc1,                myfunc1.prototype.constructor );
new TestCase( "myfunc1.arguments",                         null,                   myfunc1.arguments );
new TestCase( "myfunc1(1,2,3)",                            6,                      myfunc1(1,2,3) );
new TestCase( "var MYPROPS = ''; for ( var p in myfunc1.prototype ) { MYPROPS += p; }; MYPROPS",
	      "",
	      eval("var MYPROPS = ''; for ( var p in myfunc1.prototype ) { MYPROPS += p; }; MYPROPS") );

new TestCase( "myfunc2 = new Function('a','b','c'); myfunc.toString = Object.prototype.toString; myfunc.toString()",
	      "[object Function]",
	      myfunc2.toString() );
new TestCase( "myfunc2.length",                            3,                      myfunc2.length );
new TestCase( "myfunc2.prototype.toString()",              "[object Object]",      myfunc2.prototype.toString() );

new TestCase( "myfunc2.prototype.constructor",             myfunc2,                 myfunc2.prototype.constructor );
new TestCase( "myfunc2.arguments",                         null,                   myfunc2.arguments );
new TestCase( "myfunc2( 1000, 200, 30 )",                 1230,                    myfunc2(1000,200,30) );
new TestCase( "var MYPROPS = ''; for ( var p in myfunc2.prototype ) { MYPROPS += p; }; MYPROPS",
	      "",
	      eval("var MYPROPS = ''; for ( var p in myfunc2.prototype ) { MYPROPS += p; }; MYPROPS") );

new TestCase( "myfunc3 = new Function('a','b','c'); myfunc.toString = Object.prototype.toString; myfunc.toString()",
	      "[object Function]",
	      myfunc3.toString() );
new TestCase( "myfunc3.length",                            3,                      myfunc3.length );
new TestCase( "myfunc3.prototype.toString()",              "[object Object]",      myfunc3.prototype.toString() );
new TestCase( "myfunc3.prototype.valueOf() +''",           "[object Object]",      myfunc3.prototype.valueOf() +'' );
new TestCase( "myfunc3.prototype.constructor",             myfunc3,                 myfunc3.prototype.constructor );
new TestCase( "myfunc3.arguments",                         null,                   myfunc3.arguments );
new TestCase( "myfunc3(-100,100,NaN)",                    Number.NaN,              myfunc3(-100,100,NaN) );

new TestCase( "var MYPROPS = ''; for ( var p in myfunc3.prototype ) { MYPROPS += p; }; MYPROPS",
	      "",
	      eval("var MYPROPS = ''; for ( var p in myfunc3.prototype ) { MYPROPS += p; }; MYPROPS") );
test();
