/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.8.4.js
   ECMA Section:       11.8.4  The greater-than-or-equal operator ( >= )
   Description:


   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.8.4";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " The greater-than-or-equal operator ( >= )");

new TestCase( SECTION, "true >= false",              true,      true >= false );
new TestCase( SECTION, "false >= true",              false,     false >= true );
new TestCase( SECTION, "false >= false",             true,      false >= false );
new TestCase( SECTION, "true >= true",               true,      true >= true );

new TestCase( SECTION, "new Boolean(true) >= new Boolean(true)",     true,  new Boolean(true) >= new Boolean(true) );
new TestCase( SECTION, "new Boolean(true) >= new Boolean(false)",    true,  new Boolean(true) >= new Boolean(false) );
new TestCase( SECTION, "new Boolean(false) >= new Boolean(true)",    false,   new Boolean(false) >= new Boolean(true) );
new TestCase( SECTION, "new Boolean(false) >= new Boolean(false)",   true,  new Boolean(false) >= new Boolean(false) );

new TestCase( SECTION, "new MyObject(Infinity) >= new MyObject(Infinity)",   true,  new MyObject( Number.POSITIVE_INFINITY ) >= new MyObject( Number.POSITIVE_INFINITY) );
new TestCase( SECTION, "new MyObject(-Infinity) >= new MyObject(Infinity)",  false,   new MyObject( Number.NEGATIVE_INFINITY ) >= new MyObject( Number.POSITIVE_INFINITY) );
new TestCase( SECTION, "new MyObject(-Infinity) >= new MyObject(-Infinity)", true,  new MyObject( Number.NEGATIVE_INFINITY ) >= new MyObject( Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, "new MyValueObject(false) >= new MyValueObject(true)",  false,   new MyValueObject(false) >= new MyValueObject(true) );
new TestCase( SECTION, "new MyValueObject(true) >= new MyValueObject(true)",   true,  new MyValueObject(true) >= new MyValueObject(true) );
new TestCase( SECTION, "new MyValueObject(false) >= new MyValueObject(false)", true,  new MyValueObject(false) >= new MyValueObject(false) );

new TestCase( SECTION, "new MyStringObject(false) >= new MyStringObject(true)",  false,   new MyStringObject(false) >= new MyStringObject(true) );
new TestCase( SECTION, "new MyStringObject(true) >= new MyStringObject(true)",   true,  new MyStringObject(true) >= new MyStringObject(true) );
new TestCase( SECTION, "new MyStringObject(false) >= new MyStringObject(false)", true,  new MyStringObject(false) >= new MyStringObject(false) );

new TestCase( SECTION, "Number.NaN >= Number.NaN",   false,     Number.NaN >= Number.NaN );
new TestCase( SECTION, "0 >= Number.NaN",            false,     0 >= Number.NaN );
new TestCase( SECTION, "Number.NaN >= 0",            false,     Number.NaN >= 0 );

new TestCase( SECTION, "0 >= -0",                    true,      0 >= -0 );
new TestCase( SECTION, "-0 >= 0",                    true,      -0 >= 0 );

new TestCase( SECTION, "Infinity >= 0",                  true,      Number.POSITIVE_INFINITY >= 0 );
new TestCase( SECTION, "Infinity >= Number.MAX_VALUE",   true,      Number.POSITIVE_INFINITY >= Number.MAX_VALUE );
new TestCase( SECTION, "Infinity >= Infinity",           true,      Number.POSITIVE_INFINITY >= Number.POSITIVE_INFINITY );

new TestCase( SECTION, "0 >= Infinity",                  false,       0 >= Number.POSITIVE_INFINITY );
new TestCase( SECTION, "Number.MAX_VALUE >= Infinity",   false,       Number.MAX_VALUE >= Number.POSITIVE_INFINITY );

new TestCase( SECTION, "0 >= -Infinity",                 true,      0 >= Number.NEGATIVE_INFINITY );
new TestCase( SECTION, "Number.MAX_VALUE >= -Infinity",  true,      Number.MAX_VALUE >= Number.NEGATIVE_INFINITY );
new TestCase( SECTION, "-Infinity >= -Infinity",         true,      Number.NEGATIVE_INFINITY >= Number.NEGATIVE_INFINITY );

new TestCase( SECTION, "-Infinity >= 0",                 false,       Number.NEGATIVE_INFINITY >= 0 );
new TestCase( SECTION, "-Infinity >= -Number.MAX_VALUE", false,       Number.NEGATIVE_INFINITY >= -Number.MAX_VALUE );
new TestCase( SECTION, "-Infinity >= Number.MIN_VALUE",  false,       Number.NEGATIVE_INFINITY >= Number.MIN_VALUE );

new TestCase( SECTION, "'string' > 'string'",           false,       'string' > 'string' );
new TestCase( SECTION, "'astring' > 'string'",          false,       'astring' > 'string' );
new TestCase( SECTION, "'strings' > 'stringy'",         false,       'strings' > 'stringy' );
new TestCase( SECTION, "'strings' > 'stringier'",       true,       'strings' > 'stringier' );
new TestCase( SECTION, "'string' > 'astring'",          true,      'string' > 'astring' );
new TestCase( SECTION, "'string' > 'strings'",          false,       'string' > 'strings' );

test();

function MyObject(value) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
  this.toString = new Function( "return this.value +''" );
}
function MyValueObject(value) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
}
function MyStringObject(value) {
  this.value = value;
  this.toString = new Function( "return this.value +''" );
}
