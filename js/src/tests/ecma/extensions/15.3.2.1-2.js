/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Function Constructor";

writeHeaderToLog( SECTION + " "+ TITLE);


var myfunc1 = new Function("a","b","c", "return a+b+c" );
var myfunc2 = new Function("a, b, c",   "return a+b+c" );
var myfunc3 = new Function("a,b", "c",  "return a+b+c" );

myfunc1.toString = Object.prototype.toString;
myfunc2.toString = Object.prototype.toString;
myfunc3.toString = Object.prototype.toString;


new TestCase( SECTION,  "myfunc2.__proto__",                         Function.prototype,     myfunc2.__proto__ );

new TestCase( SECTION,  "myfunc3.__proto__",                         Function.prototype,     myfunc3.__proto__ );

test();
