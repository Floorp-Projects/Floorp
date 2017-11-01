/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.4-2-n.js
   ECMA Section:       15.9.5.4-1 Date.prototype.getTime
   Description:

   1.  If the this value is not an object whose [[Class]] property is "Date",
   generate a runtime error.
   2.  Return this time value.
   Author:             christine@netscape.com
   Date:               12 november 1997
*/


var SECTION = "15.9.5.4-2-n";
var TITLE   = "Date.prototype.getTime";

writeHeaderToLog( SECTION + " "+ TITLE);

var MYDATE = new MyDate( TIME_2000 );

DESCRIPTION = "MYDATE.getTime()";

new TestCase( "MYDATE.getTime()",
	      "error",
	      eval("MYDATE.getTime()") );

test();

function MyDate( value ) {
  this.value = value;
  this.getTime = Date.prototype.getTime;
}
