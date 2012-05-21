/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.4.3.js
   ECMA Section:       15.6.4.3 Boolean.prototype.valueOf()
   Description:        Returns this boolean value.

   The valueOf function is not generic; it generates
   a runtime error if its this value is not a Boolean
   object.  Therefore it cannot be transferred to other
   kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               june 27, 1997
*/

var SECTION = "15.6.4.3-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Boolean.prototype.valueOf()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,   "new Boolean(1)",       true,   (new Boolean(1)).valueOf() );

new TestCase( SECTION,   "new Boolean(0)",       false,  (new Boolean(0)).valueOf() );
new TestCase( SECTION,   "new Boolean(-1)",      true,   (new Boolean(-1)).valueOf() );
new TestCase( SECTION,   "new Boolean('1')",     true,   (new Boolean("1")).valueOf() );
new TestCase( SECTION,   "new Boolean('0')",     true,   (new Boolean("0")).valueOf() );
new TestCase( SECTION,   "new Boolean(true)",    true,   (new Boolean(true)).valueOf() );
new TestCase( SECTION,   "new Boolean(false)",   false,  (new Boolean(false)).valueOf() );
new TestCase( SECTION,   "new Boolean('true')",  true,   (new Boolean("true")).valueOf() );
new TestCase( SECTION,   "new Boolean('false')", true,   (new Boolean('false')).valueOf() );

new TestCase( SECTION,   "new Boolean('')",      false,  (new Boolean('')).valueOf() );
new TestCase( SECTION,   "new Boolean(null)",    false,  (new Boolean(null)).valueOf() );
new TestCase( SECTION,   "new Boolean(void(0))", false,  (new Boolean(void(0))).valueOf() );
new TestCase( SECTION,   "new Boolean(-Infinity)", true, (new Boolean(Number.NEGATIVE_INFINITY)).valueOf() );
new TestCase( SECTION,   "new Boolean(NaN)",     false,  (new Boolean(Number.NaN)).valueOf() );
new TestCase( SECTION,   "new Boolean()",        false,  (new Boolean()).valueOf() );

new TestCase( SECTION,   "new Boolean(x=1)",     true,   (new Boolean(x=1)).valueOf() );
new TestCase( SECTION,   "new Boolean(x=0)",     false,  (new Boolean(x=0)).valueOf() );
new TestCase( SECTION,   "new Boolean(x=false)", false,  (new Boolean(x=false)).valueOf() );
new TestCase( SECTION,   "new Boolean(x=true)",  true,   (new Boolean(x=true)).valueOf() );
new TestCase( SECTION,   "new Boolean(x=null)",  false,  (new Boolean(x=null)).valueOf() );
new TestCase( SECTION,   "new Boolean(x='')",    false,  (new Boolean(x="")).valueOf() );
new TestCase( SECTION,   "new Boolean(x=' ')",   true,   (new Boolean(x=" ")).valueOf() );

test();
