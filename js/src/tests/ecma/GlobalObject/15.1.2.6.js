/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
var TITLE   = "isNaN( x )";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "isNaN.length",      1,                  isNaN.length );
new TestCase( "var MYPROPS=''; for ( var p in isNaN ) { MYPROPS+= p }; MYPROPS", "", eval("var MYPROPS=''; for ( var p in isNaN ) { MYPROPS+= p }; MYPROPS") );
new TestCase( "isNaN.length = null; isNaN.length", 1,      eval("isNaN.length=null; isNaN.length") );

//     new TestCase( "isNaN.__proto__",   Function.prototype, isNaN.__proto__ );

new TestCase( "isNaN()",           true,               isNaN() );
new TestCase( "isNaN( null )",     false,              isNaN(null) );
new TestCase( "isNaN( void 0 )",   true,               isNaN(void 0) );
new TestCase( "isNaN( true )",     false,              isNaN(true) );
new TestCase( "isNaN( false)",     false,              isNaN(false) );
new TestCase( "isNaN( ' ' )",      false,              isNaN( " " ) );

new TestCase( "isNaN( 0 )",        false,              isNaN(0) );
new TestCase( "isNaN( 1 )",        false,              isNaN(1) );
new TestCase( "isNaN( 2 )",        false,              isNaN(2) );
new TestCase( "isNaN( 3 )",        false,              isNaN(3) );
new TestCase( "isNaN( 4 )",        false,              isNaN(4) );
new TestCase( "isNaN( 5 )",        false,              isNaN(5) );
new TestCase( "isNaN( 6 )",        false,              isNaN(6) );
new TestCase( "isNaN( 7 )",        false,              isNaN(7) );
new TestCase( "isNaN( 8 )",        false,              isNaN(8) );
new TestCase( "isNaN( 9 )",        false,              isNaN(9) );

new TestCase( "isNaN( '0' )",        false,              isNaN('0') );
new TestCase( "isNaN( '1' )",        false,              isNaN('1') );
new TestCase( "isNaN( '2' )",        false,              isNaN('2') );
new TestCase( "isNaN( '3' )",        false,              isNaN('3') );
new TestCase( "isNaN( '4' )",        false,              isNaN('4') );
new TestCase( "isNaN( '5' )",        false,              isNaN('5') );
new TestCase( "isNaN( '6' )",        false,              isNaN('6') );
new TestCase( "isNaN( '7' )",        false,              isNaN('7') );
new TestCase( "isNaN( '8' )",        false,              isNaN('8') );
new TestCase( "isNaN( '9' )",        false,              isNaN('9') );


new TestCase( "isNaN( 0x0a )",    false,              isNaN( 0x0a ) );
new TestCase( "isNaN( 0xaa )",    false,              isNaN( 0xaa ) );
new TestCase( "isNaN( 0x0A )",    false,              isNaN( 0x0A ) );
new TestCase( "isNaN( 0xAA )",    false,              isNaN( 0xAA ) );

new TestCase( "isNaN( '0x0a' )",    false,              isNaN( "0x0a" ) );
new TestCase( "isNaN( '0xaa' )",    false,              isNaN( "0xaa" ) );
new TestCase( "isNaN( '0x0A' )",    false,              isNaN( "0x0A" ) );
new TestCase( "isNaN( '0xAA' )",    false,              isNaN( "0xAA" ) );

new TestCase( "isNaN( 077 )",      false,              isNaN( 077 ) );
new TestCase( "isNaN( '077' )",    false,              isNaN( "077" ) );


new TestCase( "isNaN( Number.NaN )",   true,              isNaN(Number.NaN) );
new TestCase( "isNaN( Number.POSITIVE_INFINITY )", false,  isNaN(Number.POSITIVE_INFINITY) );
new TestCase( "isNaN( Number.NEGATIVE_INFINITY )", false,  isNaN(Number.NEGATIVE_INFINITY) );
new TestCase( "isNaN( Number.MAX_VALUE )",         false,  isNaN(Number.MAX_VALUE) );
new TestCase( "isNaN( Number.MIN_VALUE )",         false,  isNaN(Number.MIN_VALUE) );

new TestCase( "isNaN( NaN )",               true,      isNaN(NaN) );
new TestCase( "isNaN( Infinity )",          false,     isNaN(Infinity) );

new TestCase( "isNaN( 'Infinity' )",               false,  isNaN("Infinity") );
new TestCase( "isNaN( '-Infinity' )",              false,  isNaN("-Infinity") );

test();
