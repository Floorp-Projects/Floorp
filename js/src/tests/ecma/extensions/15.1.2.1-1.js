/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
var TITLE   = "eval(x)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "eval.length",              1,              eval.length );
new TestCase( "var PROPS = ''; for ( p in eval ) { PROPS += p }; PROPS",  "", eval("var PROPS = ''; for ( p in eval ) { PROPS += p }; PROPS") );
new TestCase( "eval.length = null; eval.length",       1, eval( "eval.length = null; eval.length") );
//     new TestCase( "eval.__proto__",                       Function.prototype,            eval.__proto__ );

// test cases where argument is not a string.  should return the argument.

new TestCase( "eval()",                                void 0,                     eval() );
new TestCase( "eval(void 0)",                          void 0,                     eval( void 0) );
new TestCase( "eval(null)",                            null,                       eval( null ) );
new TestCase( "eval(true)",                            true,                       eval( true ) );
new TestCase( "eval(false)",                           false,                      eval( false ) );

new TestCase( "typeof eval(new String('Infinity/-0')", "object",                   typeof eval(new String('Infinity/-0')) );

new TestCase( "eval([1,2,3,4,5,6])",                  "1,2,3,4,5,6",                 ""+eval([1,2,3,4,5,6]) );
new TestCase( "eval(new Array(0,1,2,3)",              "1,2,3",                       ""+  eval(new Array(1,2,3)) );
new TestCase( "eval(1)",                              1,                             eval(1) );
new TestCase( "eval(0)",                              0,                             eval(0) );
new TestCase( "eval(-1)",                             -1,                            eval(-1) );
new TestCase( "eval(Number.NaN)",                     Number.NaN,                    eval(Number.NaN) );
new TestCase( "eval(Number.MIN_VALUE)",               5e-308,                        eval(Number.MIN_VALUE) );
new TestCase( "eval(-Number.MIN_VALUE)",              -5e-308,                       eval(-Number.MIN_VALUE) );
new TestCase( "eval(Number.POSITIVE_INFINITY)",       Number.POSITIVE_INFINITY,      eval(Number.POSITIVE_INFINITY) );
new TestCase( "eval(Number.NEGATIVE_INFINITY)",       Number.NEGATIVE_INFINITY,      eval(Number.NEGATIVE_INFINITY) );
new TestCase( "eval( 4294967296 )",                   4294967296,                    eval(4294967296) );
new TestCase( "eval( 2147483648 )",                   2147483648,                    eval(2147483648) );

test();
