/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.23-3-n.js
   ECMA Section:       15.9.5.23
   Description:        Date.prototype.setTime

   1.  If the this value is not a Date object, generate a runtime error.
   2.  Call ToNumber(time).
   3.  Call TimeClip(Result(1)).
   4.  Set the [[Value]] property of the this value to Result(2).
   5.  Return the value of the [[Value]] property of the this value.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.23-3-n";
var TITLE   = "Date.prototype.setTime()";

writeHeaderToLog( SECTION + " "+ TITLE);

var MYDATE = new MyDate(TIME_1970);

DESCRIPTION = "MYDATE.setTime(TIME_2000)";

new TestCase(
	      "MYDATE.setTime(TIME_2000)",
	      "error",
	      eval("MYDATE.setTime(TIME_2000)") );

test();

function MyDate(value) {
  this.value = value;
  this.setTime = Date.prototype.setTime;
  return this;
}
