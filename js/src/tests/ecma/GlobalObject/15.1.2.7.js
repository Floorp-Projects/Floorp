/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1.2.7.js
   ECMA Section:       15.1.2.7 isFinite(number)

   Description:        Applies ToNumber to its argument, then returns false if
   the result is NaN, Infinity, or -Infinity, and otherwise
   returns true.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.1.2.7";
var TITLE   = "isFinite( x )";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "isFinite.length",      1,                  isFinite.length );
new TestCase( "isFinite.length = null; isFinite.length",   1,      eval("isFinite.length=null; isFinite.length") );
new TestCase( "var MYPROPS=''; for ( p in isFinite ) { MYPROPS+= p }; MYPROPS",    "", eval("var MYPROPS=''; for ( p in isFinite ) { MYPROPS += p }; MYPROPS") );

new TestCase( "isFinite()",           false,              isFinite() );
new TestCase( "isFinite( null )",      true,              isFinite(null) );
new TestCase( "isFinite( void 0 )",    false,             isFinite(void 0) );
new TestCase( "isFinite( false )",     true,              isFinite(false) );
new TestCase( "isFinite( true)",       true,              isFinite(true) );
new TestCase( "isFinite( ' ' )",       true,              isFinite( " " ) );

new TestCase( "isFinite( new Boolean(true) )",     true,   isFinite(new Boolean(true)) );
new TestCase( "isFinite( new Boolean(false) )",    true,   isFinite(new Boolean(false)) );

new TestCase( "isFinite( 0 )",        true,              isFinite(0) );
new TestCase( "isFinite( 1 )",        true,              isFinite(1) );
new TestCase( "isFinite( 2 )",        true,              isFinite(2) );
new TestCase( "isFinite( 3 )",        true,              isFinite(3) );
new TestCase( "isFinite( 4 )",        true,              isFinite(4) );
new TestCase( "isFinite( 5 )",        true,              isFinite(5) );
new TestCase( "isFinite( 6 )",        true,              isFinite(6) );
new TestCase( "isFinite( 7 )",        true,              isFinite(7) );
new TestCase( "isFinite( 8 )",        true,              isFinite(8) );
new TestCase( "isFinite( 9 )",        true,              isFinite(9) );

new TestCase( "isFinite( '0' )",        true,              isFinite('0') );
new TestCase( "isFinite( '1' )",        true,              isFinite('1') );
new TestCase( "isFinite( '2' )",        true,              isFinite('2') );
new TestCase( "isFinite( '3' )",        true,              isFinite('3') );
new TestCase( "isFinite( '4' )",        true,              isFinite('4') );
new TestCase( "isFinite( '5' )",        true,              isFinite('5') );
new TestCase( "isFinite( '6' )",        true,              isFinite('6') );
new TestCase( "isFinite( '7' )",        true,              isFinite('7') );
new TestCase( "isFinite( '8' )",        true,              isFinite('8') );
new TestCase( "isFinite( '9' )",        true,              isFinite('9') );

new TestCase( "isFinite( 0x0a )",    true,                 isFinite( 0x0a ) );
new TestCase( "isFinite( 0xaa )",    true,                 isFinite( 0xaa ) );
new TestCase( "isFinite( 0x0A )",    true,                 isFinite( 0x0A ) );
new TestCase( "isFinite( 0xAA )",    true,                 isFinite( 0xAA ) );

new TestCase( "isFinite( '0x0a' )",    true,               isFinite( "0x0a" ) );
new TestCase( "isFinite( '0xaa' )",    true,               isFinite( "0xaa" ) );
new TestCase( "isFinite( '0x0A' )",    true,               isFinite( "0x0A" ) );
new TestCase( "isFinite( '0xAA' )",    true,               isFinite( "0xAA" ) );

new TestCase( "isFinite( 077 )",       true,               isFinite( 077 ) );
new TestCase( "isFinite( '077' )",     true,               isFinite( "077" ) );

new TestCase( "isFinite( new String('Infinity') )",        false,      isFinite(new String("Infinity")) );
new TestCase( "isFinite( new String('-Infinity') )",       false,      isFinite(new String("-Infinity")) );

new TestCase( "isFinite( 'Infinity' )",        false,      isFinite("Infinity") );
new TestCase( "isFinite( '-Infinity' )",       false,      isFinite("-Infinity") );
new TestCase( "isFinite( Number.POSITIVE_INFINITY )",  false,  isFinite(Number.POSITIVE_INFINITY) );
new TestCase( "isFinite( Number.NEGATIVE_INFINITY )",  false,  isFinite(Number.NEGATIVE_INFINITY) );
new TestCase( "isFinite( Number.NaN )",                false,  isFinite(Number.NaN) );

new TestCase( "isFinite( Infinity )",  false,  isFinite(Infinity) );
new TestCase( "isFinite( -Infinity )",  false,  isFinite(-Infinity) );
new TestCase( "isFinite( NaN )",                false,  isFinite(NaN) );


new TestCase( "isFinite( Number.MAX_VALUE )",          true,  isFinite(Number.MAX_VALUE) );
new TestCase( "isFinite( Number.MIN_VALUE )",          true,  isFinite(Number.MIN_VALUE) );

test();
