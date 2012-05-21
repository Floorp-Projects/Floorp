/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1.2.6.js
   ECMA Section:       15.1.2.6 isNaN( x )

   Description:        Applies ToNumber to its argument, then returns true if
   the result isNaN and otherwise returns false.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.1.2.6";
var VERSION = "ECMA_1";
var TITLE   = "isNaN( x )";
var BUGNUMBER = "none";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "isNaN.length",      1,                  isNaN.length );
new TestCase( SECTION, "var MYPROPS=''; for ( var p in isNaN ) { MYPROPS+= p }; MYPROPS", "", eval("var MYPROPS=''; for ( var p in isNaN ) { MYPROPS+= p }; MYPROPS") );
new TestCase( SECTION, "isNaN.length = null; isNaN.length", 1,      eval("isNaN.length=null; isNaN.length") );
new TestCase( SECTION, "delete isNaN.length",               false,  delete isNaN.length );
new TestCase( SECTION, "delete isNaN.length; isNaN.length", 1,      eval("delete isNaN.length; isNaN.length") );

//     new TestCase( SECTION, "isNaN.__proto__",   Function.prototype, isNaN.__proto__ );

new TestCase( SECTION, "isNaN()",           true,               isNaN() );
new TestCase( SECTION, "isNaN( null )",     false,              isNaN(null) );
new TestCase( SECTION, "isNaN( void 0 )",   true,               isNaN(void 0) );
new TestCase( SECTION, "isNaN( true )",     false,              isNaN(true) );
new TestCase( SECTION, "isNaN( false)",     false,              isNaN(false) );
new TestCase( SECTION, "isNaN( ' ' )",      false,              isNaN( " " ) );

new TestCase( SECTION, "isNaN( 0 )",        false,              isNaN(0) );
new TestCase( SECTION, "isNaN( 1 )",        false,              isNaN(1) );
new TestCase( SECTION, "isNaN( 2 )",        false,              isNaN(2) );
new TestCase( SECTION, "isNaN( 3 )",        false,              isNaN(3) );
new TestCase( SECTION, "isNaN( 4 )",        false,              isNaN(4) );
new TestCase( SECTION, "isNaN( 5 )",        false,              isNaN(5) );
new TestCase( SECTION, "isNaN( 6 )",        false,              isNaN(6) );
new TestCase( SECTION, "isNaN( 7 )",        false,              isNaN(7) );
new TestCase( SECTION, "isNaN( 8 )",        false,              isNaN(8) );
new TestCase( SECTION, "isNaN( 9 )",        false,              isNaN(9) );

new TestCase( SECTION, "isNaN( '0' )",        false,              isNaN('0') );
new TestCase( SECTION, "isNaN( '1' )",        false,              isNaN('1') );
new TestCase( SECTION, "isNaN( '2' )",        false,              isNaN('2') );
new TestCase( SECTION, "isNaN( '3' )",        false,              isNaN('3') );
new TestCase( SECTION, "isNaN( '4' )",        false,              isNaN('4') );
new TestCase( SECTION, "isNaN( '5' )",        false,              isNaN('5') );
new TestCase( SECTION, "isNaN( '6' )",        false,              isNaN('6') );
new TestCase( SECTION, "isNaN( '7' )",        false,              isNaN('7') );
new TestCase( SECTION, "isNaN( '8' )",        false,              isNaN('8') );
new TestCase( SECTION, "isNaN( '9' )",        false,              isNaN('9') );


new TestCase( SECTION, "isNaN( 0x0a )",    false,              isNaN( 0x0a ) );
new TestCase( SECTION, "isNaN( 0xaa )",    false,              isNaN( 0xaa ) );
new TestCase( SECTION, "isNaN( 0x0A )",    false,              isNaN( 0x0A ) );
new TestCase( SECTION, "isNaN( 0xAA )",    false,              isNaN( 0xAA ) );

new TestCase( SECTION, "isNaN( '0x0a' )",    false,              isNaN( "0x0a" ) );
new TestCase( SECTION, "isNaN( '0xaa' )",    false,              isNaN( "0xaa" ) );
new TestCase( SECTION, "isNaN( '0x0A' )",    false,              isNaN( "0x0A" ) );
new TestCase( SECTION, "isNaN( '0xAA' )",    false,              isNaN( "0xAA" ) );

new TestCase( SECTION, "isNaN( 077 )",      false,              isNaN( 077 ) );
new TestCase( SECTION, "isNaN( '077' )",    false,              isNaN( "077" ) );


new TestCase( SECTION, "isNaN( Number.NaN )",   true,              isNaN(Number.NaN) );
new TestCase( SECTION, "isNaN( Number.POSITIVE_INFINITY )", false,  isNaN(Number.POSITIVE_INFINITY) );
new TestCase( SECTION, "isNaN( Number.NEGATIVE_INFINITY )", false,  isNaN(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION, "isNaN( Number.MAX_VALUE )",         false,  isNaN(Number.MAX_VALUE) );
new TestCase( SECTION, "isNaN( Number.MIN_VALUE )",         false,  isNaN(Number.MIN_VALUE) );

new TestCase( SECTION, "isNaN( NaN )",               true,      isNaN(NaN) );
new TestCase( SECTION, "isNaN( Infinity )",          false,     isNaN(Infinity) );

new TestCase( SECTION, "isNaN( 'Infinity' )",               false,  isNaN("Infinity") );
new TestCase( SECTION, "isNaN( '-Infinity' )",              false,  isNaN("-Infinity") );

test();
