/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          typeof_1.js
   ECMA Section:       11.4.3 typeof operator
   Description:        typeof evaluates unary expressions:
   undefined   "undefined"
   null        "object"
   Boolean     "boolean"
   Number      "number"
   String      "string"
   Object      "object" [native, doesn't implement Call]
   Object      "function" [native, implements [Call]]
   Object      implementation dependent
   [not sure how to test this]
   Author:             christine@netscape.com
   Date:               june 30, 1997

*/

var SECTION = "11.4.3";

var VERSION = "ECMA_1";
startTest();
var TITLE   = " The typeof operator";
writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,     "typeof(void(0))",              "undefined",        typeof(void(0)) );
new TestCase( SECTION,     "typeof(null)",                 "object",           typeof(null) );
new TestCase( SECTION,     "typeof(true)",                 "boolean",          typeof(true) );
new TestCase( SECTION,     "typeof(false)",                "boolean",          typeof(false) );
new TestCase( SECTION,     "typeof(new Boolean())",        "object",           typeof(new Boolean()) );
new TestCase( SECTION,     "typeof(new Boolean(true))",    "object",           typeof(new Boolean(true)) );
new TestCase( SECTION,     "typeof(Boolean())",            "boolean",          typeof(Boolean()) );
new TestCase( SECTION,     "typeof(Boolean(false))",       "boolean",          typeof(Boolean(false)) );
new TestCase( SECTION,     "typeof(Boolean(true))",        "boolean",          typeof(Boolean(true)) );
new TestCase( SECTION,     "typeof(NaN)",                  "number",           typeof(Number.NaN) );
new TestCase( SECTION,     "typeof(Infinity)",             "number",           typeof(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,     "typeof(-Infinity)",            "number",           typeof(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,     "typeof(Math.PI)",              "number",           typeof(Math.PI) );
new TestCase( SECTION,     "typeof(0)",                    "number",           typeof(0) );
new TestCase( SECTION,     "typeof(1)",                    "number",           typeof(1) );
new TestCase( SECTION,     "typeof(-1)",                   "number",           typeof(-1) );
new TestCase( SECTION,     "typeof('0')",                  "string",           typeof("0") );
new TestCase( SECTION,     "typeof(Number())",             "number",           typeof(Number()) );
new TestCase( SECTION,     "typeof(Number(0))",            "number",           typeof(Number(0)) );
new TestCase( SECTION,     "typeof(Number(1))",            "number",           typeof(Number(1)) );
new TestCase( SECTION,     "typeof(Nubmer(-1))",           "number",           typeof(Number(-1)) );
new TestCase( SECTION,     "typeof(new Number())",         "object",           typeof(new Number()) );
new TestCase( SECTION,     "typeof(new Number(0))",        "object",           typeof(new Number(0)) );
new TestCase( SECTION,     "typeof(new Number(1))",        "object",           typeof(new Number(1)) );

// Math does not implement [[Construct]] or [[Call]] so its type is object.

new TestCase( SECTION,     "typeof(Math)",                 "object",         typeof(Math) );

new TestCase( SECTION,     "typeof(Number.prototype.toString)", "function",    typeof(Number.prototype.toString) );

new TestCase( SECTION,     "typeof('a string')",           "string",           typeof("a string") );
new TestCase( SECTION,     "typeof('')",                   "string",           typeof("") );
new TestCase( SECTION,     "typeof(new Date())",           "object",           typeof(new Date()) );
new TestCase( SECTION,     "typeof(new Array(1,2,3))",     "object",           typeof(new Array(1,2,3)) );
new TestCase( SECTION,     "typeof(new String('string object'))",  "object",   typeof(new String("string object")) );
new TestCase( SECTION,     "typeof(String('string primitive'))",    "string",  typeof(String("string primitive")) );
new TestCase( SECTION,     "typeof(['array', 'of', 'strings'])",   "object",   typeof(["array", "of", "strings"]) );
new TestCase( SECTION,     "typeof(new Function())",                "function",     typeof( new Function() ) );
new TestCase( SECTION,     "typeof(parseInt)",                      "function",     typeof( parseInt ) );
new TestCase( SECTION,     "typeof(test)",                          "function",     typeof( test ) );
new TestCase( SECTION,     "typeof(String.fromCharCode)",           "function",     typeof( String.fromCharCode )  );


test();

