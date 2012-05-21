/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

new TestCase( SECTION, "Object(true).valueOf()",    true,                   (Object(true)).valueOf() );
new TestCase( SECTION, "typeof Object(true)",       "object",               typeof Object(true) );

new TestCase( SECTION, "Object(false).valueOf()",    false,                  (Object(false)).valueOf() );
new TestCase( SECTION, "typeof Object(false)",      "object",               typeof Object(false) );

new TestCase( SECTION, "Object(0).valueOf()",       0,                      (Object(0)).valueOf() );
new TestCase( SECTION, "typeof Object(0)",          "object",               typeof Object(0) );

new TestCase( SECTION, "Object(-0).valueOf()",      -0,                     (Object(-0)).valueOf() );
new TestCase( SECTION, "typeof Object(-0)",         "object",               typeof Object(-0) );

new TestCase( SECTION, "Object(1).valueOf()",       1,                      (Object(1)).valueOf() );
new TestCase( SECTION, "typeof Object(1)",          "object",               typeof Object(1) );

new TestCase( SECTION, "Object(-1).valueOf()",      -1,                     (Object(-1)).valueOf() );
new TestCase( SECTION, "typeof Object(-1)",         "object",               typeof Object(-1) );

new TestCase( SECTION, "Object(Number.MAX_VALUE).valueOf()",    1.7976931348623157e308,         (Object(Number.MAX_VALUE)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.MAX_VALUE)",       "object",                       typeof Object(Number.MAX_VALUE) );

new TestCase( SECTION, "Object(Number.MIN_VALUE).valueOf()",     5e-324,           (Object(Number.MIN_VALUE)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.MIN_VALUE)",       "object",         typeof Object(Number.MIN_VALUE) );

new TestCase( SECTION, "Object(Number.POSITIVE_INFINITY).valueOf()",    Number.POSITIVE_INFINITY,       (Object(Number.POSITIVE_INFINITY)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.POSITIVE_INFINITY)",       "object",                       typeof Object(Number.POSITIVE_INFINITY) );

new TestCase( SECTION, "Object(Number.NEGATIVE_INFINITY).valueOf()",    Number.NEGATIVE_INFINITY,       (Object(Number.NEGATIVE_INFINITY)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.NEGATIVE_INFINITY)",       "object",            typeof Object(Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, "Object(Number.NaN).valueOf()",      Number.NaN,                (Object(Number.NaN)).valueOf() );
new TestCase( SECTION, "typeof Object(Number.NaN)",         "object",                  typeof Object(Number.NaN) );

new TestCase( SECTION, "Object('a string').valueOf()",      "a string",         (Object("a string")).valueOf() );
new TestCase( SECTION, "typeof Object('a string')",         "object",           typeof (Object("a string")) );

new TestCase( SECTION, "Object('').valueOf()",              "",                 (Object("")).valueOf() );
new TestCase( SECTION, "typeof Object('')",                 "object",           typeof (Object("")) );

new TestCase( SECTION, "Object('\\r\\t\\b\\n\\v\\f').valueOf()",   "\r\t\b\n\v\f",   (Object("\r\t\b\n\v\f")).valueOf() );
new TestCase( SECTION, "typeof Object('\\r\\t\\b\\n\\v\\f')",      "object",           typeof (Object("\\r\\t\\b\\n\\v\\f")) );

new TestCase( SECTION,  "Object( '\\\'\\\"\\' ).valueOf()",      "\'\"\\",          (Object("\'\"\\")).valueOf() );
new TestCase( SECTION,  "typeof Object( '\\\'\\\"\\' )",        "object",           typeof Object("\'\"\\") );

new TestCase( SECTION, "Object( new MyObject(true) ).valueOf()",    true,           eval("Object( new MyObject(true) ).valueOf()") );
new TestCase( SECTION, "typeof Object( new MyObject(true) )",       "object",       eval("typeof Object( new MyObject(true) )") );
new TestCase( SECTION, "(Object( new MyObject(true) )).toString()",  "[object Object]",       eval("(Object( new MyObject(true) )).toString()") );

test();

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function ( "return this.value" );
}
