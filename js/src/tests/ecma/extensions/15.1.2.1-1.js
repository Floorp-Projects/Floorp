/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1.2.1-1.js
   ECMA Section:       15.1.2.1 eval(x)

   if x is not a string object, return x.
   Description:
   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.1.2.1-1";
var VERSION = "ECMA_1";
var TITLE   = "eval(x)";
var BUGNUMBER = "none";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,      "eval.length",              1,              eval.length );
new TestCase( SECTION,      "delete eval.length",       false,          delete eval.length );
new TestCase( SECTION,      "var PROPS = ''; for ( p in eval ) { PROPS += p }; PROPS",  "", eval("var PROPS = ''; for ( p in eval ) { PROPS += p }; PROPS") );
new TestCase( SECTION,      "eval.length = null; eval.length",       1, eval( "eval.length = null; eval.length") );
//     new TestCase( SECTION,     "eval.__proto__",                       Function.prototype,            eval.__proto__ );

// test cases where argument is not a string.  should return the argument.

new TestCase( SECTION,     "eval()",                                void 0,                     eval() );
new TestCase( SECTION,     "eval(void 0)",                          void 0,                     eval( void 0) );
new TestCase( SECTION,     "eval(null)",                            null,                       eval( null ) );
new TestCase( SECTION,     "eval(true)",                            true,                       eval( true ) );
new TestCase( SECTION,     "eval(false)",                           false,                      eval( false ) );

new TestCase( SECTION,     "typeof eval(new String('Infinity/-0')", "object",                   typeof eval(new String('Infinity/-0')) );

new TestCase( SECTION,     "eval([1,2,3,4,5,6])",                  "1,2,3,4,5,6",                 ""+eval([1,2,3,4,5,6]) );
new TestCase( SECTION,     "eval(new Array(0,1,2,3)",              "1,2,3",                       ""+  eval(new Array(1,2,3)) );
new TestCase( SECTION,     "eval(1)",                              1,                             eval(1) );
new TestCase( SECTION,     "eval(0)",                              0,                             eval(0) );
new TestCase( SECTION,     "eval(-1)",                             -1,                            eval(-1) );
new TestCase( SECTION,     "eval(Number.NaN)",                     Number.NaN,                    eval(Number.NaN) );
new TestCase( SECTION,     "eval(Number.MIN_VALUE)",               5e-308,                        eval(Number.MIN_VALUE) );
new TestCase( SECTION,     "eval(-Number.MIN_VALUE)",              -5e-308,                       eval(-Number.MIN_VALUE) );
new TestCase( SECTION,     "eval(Number.POSITIVE_INFINITY)",       Number.POSITIVE_INFINITY,      eval(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,     "eval(Number.NEGATIVE_INFINITY)",       Number.NEGATIVE_INFINITY,      eval(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,     "eval( 4294967296 )",                   4294967296,                    eval(4294967296) );
new TestCase( SECTION,     "eval( 2147483648 )",                   2147483648,                    eval(2147483648) );

test();
