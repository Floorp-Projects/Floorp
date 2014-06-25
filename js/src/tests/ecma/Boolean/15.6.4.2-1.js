/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.4.2.js
   ECMA Section:       15.6.4.2-1 Boolean.prototype.toString()
   Description:        If this boolean value is true, then the string "true"
   is returned; otherwise this boolean value must be false,
   and the string "false" is returned.

   The toString function is not generic; it generates
   a runtime error if its this value is not a Boolean
   object.  Therefore it cannot be transferred to other
   kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               june 27, 1997
*/

var SECTION = "15.6.4.2-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Boolean.prototype.toString()"
  writeHeaderToLog( SECTION + TITLE );


new TestCase( SECTION,   "new Boolean(1)",       "true",   (new Boolean(1)).toString() );
new TestCase( SECTION,   "new Boolean(0)",       "false",  (new Boolean(0)).toString() );
new TestCase( SECTION,   "new Boolean(-1)",      "true",   (new Boolean(-1)).toString() );
new TestCase( SECTION,   "new Boolean('1')",     "true",   (new Boolean("1")).toString() );
new TestCase( SECTION,   "new Boolean('0')",     "true",   (new Boolean("0")).toString() );
new TestCase( SECTION,   "new Boolean(true)",    "true",   (new Boolean(true)).toString() );
new TestCase( SECTION,   "new Boolean(false)",   "false",  (new Boolean(false)).toString() );
new TestCase( SECTION,   "new Boolean('true')",  "true",   (new Boolean('true')).toString() );
new TestCase( SECTION,   "new Boolean('false')", "true",   (new Boolean('false')).toString() );

new TestCase( SECTION,   "new Boolean('')",      "false",  (new Boolean('')).toString() );
new TestCase( SECTION,   "new Boolean(null)",    "false",  (new Boolean(null)).toString() );
new TestCase( SECTION,   "new Boolean(void(0))", "false",  (new Boolean(void(0))).toString() );
new TestCase( SECTION,   "new Boolean(-Infinity)", "true", (new Boolean(Number.NEGATIVE_INFINITY)).toString() );
new TestCase( SECTION,   "new Boolean(NaN)",     "false",  (new Boolean(Number.NaN)).toString() );
new TestCase( SECTION,   "new Boolean()",        "false",  (new Boolean()).toString() );
new TestCase( SECTION,   "new Boolean(x=1)",     "true",   (new Boolean(x=1)).toString() );
new TestCase( SECTION,   "new Boolean(x=0)",     "false",  (new Boolean(x=0)).toString() );
new TestCase( SECTION,   "new Boolean(x=false)", "false",  (new Boolean(x=false)).toString() );
new TestCase( SECTION,   "new Boolean(x=true)",  "true",   (new Boolean(x=true)).toString() );
new TestCase( SECTION,   "new Boolean(x=null)",  "false",  (new Boolean(x=null)).toString() );
new TestCase( SECTION,   "new Boolean(x='')",    "false",  (new Boolean(x="")).toString() );
new TestCase( SECTION,   "new Boolean(x=' ')",   "true",   (new Boolean(x=" ")).toString() );

new TestCase( SECTION,   "new Boolean(new MyObject(true))",     "true",   (new Boolean(new MyObject(true))).toString() );
new TestCase( SECTION,   "new Boolean(new MyObject(false))",    "true",   (new Boolean(new MyObject(false))).toString() );

test();

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
  return this;
}
