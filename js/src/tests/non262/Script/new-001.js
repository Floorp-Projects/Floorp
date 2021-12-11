/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          new-001.js
   Section:
   Description:

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=76103

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "new-001";
var TITLE   = "new-001";
var BUGNUMBER="31567";

printBugNumber(BUGNUMBER);
writeHeaderToLog( SECTION + " "+ TITLE);

function Test_One (x) {
  this.v = x+1;
  return x*2
    }

function Test_Two( x, y ) {
  this.v = x;
  return y;
}

new TestCase(
  "Test_One(18)",
  36,
  Test_One(18) );

new TestCase(
  "new Test_One(18)",
  "[object Object]",
  new Test_One(18) +"" );

new TestCase(
  "new Test_One(18).v",
  19,
  new Test_One(18).v );

new TestCase(
  "Test_Two(2,7)",
  7,
  Test_Two(2,7) );

new TestCase(
  "new Test_Two(2,7)",
  "[object Object]",
  new Test_Two(2,7) +"" );

new TestCase(
  "new Test_Two(2,7).v",
  2,
  new Test_Two(2,7).v );

new TestCase(
  "new (Function)(\"x\", \"return x+3\")(5,6)",
  8,
  new (Function)("x","return x+3")(5,6) );

new TestCase(
  "new new Test_Two(String, 2).v(0123)",
  "83",
  new new Test_Two(String, 2).v(0123) +"");

new TestCase(
  "new new Test_Two(String, 2).v(0123).length",
  2,
  new new Test_Two(String, 2).v(0123).length );

test();
