/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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

var TITLE   = " The typeof operator";
writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "typeof(void(0))",              "undefined",        typeof(void(0)) );
new TestCase( "typeof(null)",                 "object",           typeof(null) );
new TestCase( "typeof(true)",                 "boolean",          typeof(true) );
new TestCase( "typeof(false)",                "boolean",          typeof(false) );
new TestCase( "typeof(new Boolean())",        "object",           typeof(new Boolean()) );
new TestCase( "typeof(new Boolean(true))",    "object",           typeof(new Boolean(true)) );
new TestCase( "typeof(Boolean())",            "boolean",          typeof(Boolean()) );
new TestCase( "typeof(Boolean(false))",       "boolean",          typeof(Boolean(false)) );
new TestCase( "typeof(Boolean(true))",        "boolean",          typeof(Boolean(true)) );
new TestCase( "typeof(NaN)",                  "number",           typeof(Number.NaN) );
new TestCase( "typeof(Infinity)",             "number",           typeof(Number.POSITIVE_INFINITY) );
new TestCase( "typeof(-Infinity)",            "number",           typeof(Number.NEGATIVE_INFINITY) );
new TestCase( "typeof(Math.PI)",              "number",           typeof(Math.PI) );
new TestCase( "typeof(0)",                    "number",           typeof(0) );
new TestCase( "typeof(1)",                    "number",           typeof(1) );
new TestCase( "typeof(-1)",                   "number",           typeof(-1) );
new TestCase( "typeof('0')",                  "string",           typeof("0") );
new TestCase( "typeof(Number())",             "number",           typeof(Number()) );
new TestCase( "typeof(Number(0))",            "number",           typeof(Number(0)) );
new TestCase( "typeof(Number(1))",            "number",           typeof(Number(1)) );
new TestCase( "typeof(Nubmer(-1))",           "number",           typeof(Number(-1)) );
new TestCase( "typeof(new Number())",         "object",           typeof(new Number()) );
new TestCase( "typeof(new Number(0))",        "object",           typeof(new Number(0)) );
new TestCase( "typeof(new Number(1))",        "object",           typeof(new Number(1)) );

// Math does not implement [[Construct]] or [[Call]] so its type is object.

new TestCase( "typeof(Math)",                 "object",         typeof(Math) );

new TestCase( "typeof(Number.prototype.toString)", "function",    typeof(Number.prototype.toString) );

new TestCase( "typeof('a string')",           "string",           typeof("a string") );
new TestCase( "typeof('')",                   "string",           typeof("") );
new TestCase( "typeof(new Date())",           "object",           typeof(new Date()) );
new TestCase( "typeof(new Array(1,2,3))",     "object",           typeof(new Array(1,2,3)) );
new TestCase( "typeof(new String('string object'))",  "object",   typeof(new String("string object")) );
new TestCase( "typeof(String('string primitive'))",    "string",  typeof(String("string primitive")) );
new TestCase( "typeof(['array', 'of', 'strings'])",   "object",   typeof(["array", "of", "strings"]) );
new TestCase( "typeof(new Function())",                "function",     typeof( new Function() ) );
new TestCase( "typeof(parseInt)",                      "function",     typeof( parseInt ) );
new TestCase( "typeof(test)",                          "function",     typeof( test ) );
new TestCase( "typeof(String.fromCharCode)",           "function",     typeof( String.fromCharCode )  );


test();

