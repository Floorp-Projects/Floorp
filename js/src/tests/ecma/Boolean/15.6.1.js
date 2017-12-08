/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:      15.6.1.js
   ECMA Section:   15.6.1 The Boolean Function
   15.6.1.1 Boolean( value )
   15.6.1.2 Boolean ()
   Description:    Boolean( value ) should return a Boolean value
   not a Boolean object) computed by
   Boolean.toBooleanValue( value)

   15.6.1.2 Boolean() returns false

   Author:         christine@netscape.com
   Date:           27 jun 1997


   Data File Fields:
   VALUE       Argument passed to the Boolean function
   TYPE        typeof VALUE (not used, but helpful in understanding
   the data file)
   E_RETURN    Expected return value of Boolean( VALUE )
*/
var SECTION = "15.6.1";
var TITLE   = "The Boolean constructor called as a function: Boolean( value ) and Boolean()";

writeHeaderToLog( SECTION + " "+ TITLE);

var array = new Array();
var item = 0;

new TestCase( "Boolean(1)",         true,   Boolean(1) );
new TestCase( "Boolean(0)",         false,  Boolean(0) );
new TestCase( "Boolean(-1)",        true,   Boolean(-1) );
new TestCase( "Boolean('1')",       true,   Boolean("1") );
new TestCase( "Boolean('0')",       true,   Boolean("0") );
new TestCase( "Boolean('-1')",      true,   Boolean("-1") );
new TestCase( "Boolean(true)",      true,   Boolean(true) );
new TestCase( "Boolean(false)",     false,  Boolean(false) );

new TestCase( "Boolean('true')",    true,   Boolean("true") );
new TestCase( "Boolean('false')",   true,   Boolean("false") );
new TestCase( "Boolean(null)",      false,  Boolean(null) );

new TestCase( "Boolean(-Infinity)", true,   Boolean(Number.NEGATIVE_INFINITY) );
new TestCase( "Boolean(NaN)",       false,  Boolean(Number.NaN) );
new TestCase( "Boolean(void(0))",   false,  Boolean( void(0) ) );
new TestCase( "Boolean(x=0)",       false,  Boolean( x=0 ) );
new TestCase( "Boolean(x=1)",       true,   Boolean( x=1 ) );
new TestCase( "Boolean(x=false)",   false,  Boolean( x=false ) );
new TestCase( "Boolean(x=true)",    true,   Boolean( x=true ) );
new TestCase( "Boolean(x=null)",    false,  Boolean( x=null ) );
new TestCase( "Boolean()",          false,  Boolean() );
//    array[item++] = new TestCase( "Boolean(var someVar)",     false,  Boolean( someVar ) );

test();
