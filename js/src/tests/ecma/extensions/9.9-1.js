/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          9.9-1.js
   ECMA Section:       9.9  Type Conversion:  ToObject
   Description:

   undefined   generate a runtime error
   null        generate a runtime error
   boolean     create a new Boolean object whose default
   value is the value of the boolean.
   number      Create a new Number object whose default
   value is the value of the number.
   string      Create a new String object whose default
   value is the value of the string.
   object      Return the input argument (no conversion).
   Author:             christine@netscape.com
   Date:               17 july 1997
*/

var VERSION = "ECMA_1";
startTest();
var SECTION = "9.9-1";

writeHeaderToLog( SECTION + " Type Conversion: ToObject" );

new TestCase( SECTION, "(Object(true)).__proto__",  Boolean.prototype,      (Object(true)).__proto__ );

new TestCase( SECTION, "(Object(true)).__proto__",  Boolean.prototype,      (Object(true)).__proto__ );

new TestCase( SECTION, "(Object(0)).__proto__",     Number.prototype,      (Object(0)).__proto__ );

new TestCase( SECTION, "(Object(-0)).__proto__",    Number.prototype,      (Object(-0)).__proto__ );

new TestCase( SECTION, "(Object(1)).__proto__",     Number.prototype,      (Object(1)).__proto__ );

new TestCase( SECTION, "(Object(-1)).__proto__",    Number.prototype,      (Object(-1)).__proto__ );

new TestCase( SECTION, "(Object(Number.MAX_VALUE)).__proto__",  Number.prototype,               (Object(Number.MAX_VALUE)).__proto__ );

new TestCase( SECTION, "(Object(Number.MIN_VALUE)).__proto__",  Number.prototype, (Object(Number.MIN_VALUE)).__proto__ );

new TestCase( SECTION, "(Object(Number.POSITIVE_INFINITY)).__proto__",  Number.prototype,               (Object(Number.POSITIVE_INFINITY)).__proto__ );

new TestCase( SECTION, "(Object(Number.NEGATIVE_INFINITY)).__proto__",  Number.prototype,   (Object(Number.NEGATIVE_INFINITY)).__proto__ );

new TestCase( SECTION, "(Object(Number.NaN)).__proto__",    Number.prototype,          (Object(Number.NaN)).__proto__ );

new TestCase( SECTION, "(Object('a string')).__proto__",    String.prototype,   (Object("a string")).__proto__ );

new TestCase( SECTION, "(Object('')).__proto__",            String.prototype,   (Object("")).__proto__ );

new TestCase( SECTION, "(Object('\\r\\t\\b\\n\\v\\f')).__proto__", String.prototype,   (Object("\\r\\t\\b\\n\\v\\f")).__proto__ );

new TestCase( SECTION,  "Object( '\\\'\\\"\\' ).__proto__",      String.prototype,   (Object("\'\"\\")).__proto__ );

new TestCase( SECTION, "(Object( new MyObject(true) )).toString()",  "[object Object]",       eval("(Object( new MyObject(true) )).toString()") );

test();

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function ( "return this.value" );
}
