/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.3-2.js
   ECMA Section:       15.9.5.3-2 Date.prototype.valueOf
   Description:

   The valueOf function returns a number, which is this time value.

   The valueOf function is not generic; it generates a runtime error if
   its this value is not a Date object.  Therefore it cannot be transferred
   to other kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.3-2";
var TITLE   = "Date.prototype.valueOf";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );
addTestCase( UTC_JAN_1_2005 );

test();

function addTestCase( t ) {
  new TestCase( "(new Date("+t+").valueOf()",
		t,
		(new Date(t)).valueOf() );

  new TestCase( "(new Date("+(t+1)+").valueOf()",
		t+1,
		(new Date(t+1)).valueOf() );

  new TestCase( "(new Date("+(t-1)+").valueOf()",
		t-1,
		(new Date(t-1)).valueOf() );

  new TestCase( "(new Date("+(t-TZ_ADJUST)+").valueOf()",
		t-TZ_ADJUST,
		(new Date(t-TZ_ADJUST)).valueOf() );

  new TestCase( "(new Date("+(t+TZ_ADJUST)+").valueOf()",
		t+TZ_ADJUST,
		(new Date(t+TZ_ADJUST)).valueOf() );
}

function MyObject( value ) {
  this.value = value;
  this.valueOf = Date.prototype.valueOf;
  this.toString = new Function( "return this+\"\";");
  return this;
}
