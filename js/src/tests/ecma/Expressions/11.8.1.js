/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.8.1.js
   ECMA Section:       11.8.1  The less-than operator ( < )
   Description:


   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.8.1";

writeHeaderToLog( SECTION + " The less-than operator ( < )");

new TestCase( "true < false",              false,      true < false );
new TestCase( "false < true",              true,       false < true );
new TestCase( "false < false",             false,      false < false );
new TestCase( "true < true",               false,      true < true );

new TestCase( "new Boolean(true) < new Boolean(true)",     false,  new Boolean(true) < new Boolean(true) );
new TestCase( "new Boolean(true) < new Boolean(false)",    false,  new Boolean(true) < new Boolean(false) );
new TestCase( "new Boolean(false) < new Boolean(true)",    true,   new Boolean(false) < new Boolean(true) );
new TestCase( "new Boolean(false) < new Boolean(false)",   false,  new Boolean(false) < new Boolean(false) );

new TestCase( "new MyObject(Infinity) < new MyObject(Infinity)",   false,  new MyObject( Number.POSITIVE_INFINITY ) < new MyObject( Number.POSITIVE_INFINITY) );
new TestCase( "new MyObject(-Infinity) < new MyObject(Infinity)",  true,   new MyObject( Number.NEGATIVE_INFINITY ) < new MyObject( Number.POSITIVE_INFINITY) );
new TestCase( "new MyObject(-Infinity) < new MyObject(-Infinity)", false,  new MyObject( Number.NEGATIVE_INFINITY ) < new MyObject( Number.NEGATIVE_INFINITY) );

new TestCase( "new MyValueObject(false) < new MyValueObject(true)",  true,   new MyValueObject(false) < new MyValueObject(true) );
new TestCase( "new MyValueObject(true) < new MyValueObject(true)",   false,  new MyValueObject(true) < new MyValueObject(true) );
new TestCase( "new MyValueObject(false) < new MyValueObject(false)", false,  new MyValueObject(false) < new MyValueObject(false) );

new TestCase( "new MyStringObject(false) < new MyStringObject(true)",  true,   new MyStringObject(false) < new MyStringObject(true) );
new TestCase( "new MyStringObject(true) < new MyStringObject(true)",   false,  new MyStringObject(true) < new MyStringObject(true) );
new TestCase( "new MyStringObject(false) < new MyStringObject(false)", false,  new MyStringObject(false) < new MyStringObject(false) );

new TestCase( "Number.NaN < Number.NaN",   false,     Number.NaN < Number.NaN );
new TestCase( "0 < Number.NaN",            false,     0 < Number.NaN );
new TestCase( "Number.NaN < 0",            false,     Number.NaN < 0 );

new TestCase( "0 < -0",                    false,      0 < -0 );
new TestCase( "-0 < 0",                    false,      -0 < 0 );

new TestCase( "Infinity < 0",                  false,      Number.POSITIVE_INFINITY < 0 );
new TestCase( "Infinity < Number.MAX_VALUE",   false,      Number.POSITIVE_INFINITY < Number.MAX_VALUE );
new TestCase( "Infinity < Infinity",           false,      Number.POSITIVE_INFINITY < Number.POSITIVE_INFINITY );

new TestCase( "0 < Infinity",                  true,       0 < Number.POSITIVE_INFINITY );
new TestCase( "Number.MAX_VALUE < Infinity",   true,       Number.MAX_VALUE < Number.POSITIVE_INFINITY );

new TestCase( "0 < -Infinity",                 false,      0 < Number.NEGATIVE_INFINITY );
new TestCase( "Number.MAX_VALUE < -Infinity",  false,      Number.MAX_VALUE < Number.NEGATIVE_INFINITY );
new TestCase( "-Infinity < -Infinity",         false,      Number.NEGATIVE_INFINITY < Number.NEGATIVE_INFINITY );

new TestCase( "-Infinity < 0",                 true,       Number.NEGATIVE_INFINITY < 0 );
new TestCase( "-Infinity < -Number.MAX_VALUE", true,       Number.NEGATIVE_INFINITY < -Number.MAX_VALUE );
new TestCase( "-Infinity < Number.MIN_VALUE",  true,       Number.NEGATIVE_INFINITY < Number.MIN_VALUE );

new TestCase( "'string' < 'string'",           false,       'string' < 'string' );
new TestCase( "'astring' < 'string'",          true,       'astring' < 'string' );
new TestCase( "'strings' < 'stringy'",         true,       'strings' < 'stringy' );
new TestCase( "'strings' < 'stringier'",       false,       'strings' < 'stringier' );
new TestCase( "'string' < 'astring'",          false,      'string' < 'astring' );
new TestCase( "'string' < 'strings'",          true,       'string' < 'strings' );

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
