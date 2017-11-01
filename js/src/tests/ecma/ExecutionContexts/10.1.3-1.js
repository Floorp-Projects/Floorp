/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          10.1.3-1.js
   ECMA Section:       10.1.3
   Description:

   For each formal parameter, as defined in the FormalParameterList, create
   a property of the variable object whose name is the Identifier and whose
   attributes are determined by the type of code. The values of the
   parameters are supplied by the caller. If the caller supplies fewer
   parameter values than there are formal parameters, the extra formal
   parameters have value undefined. If two or more formal parameters share
   the same name, hence the same property, the corresponding property is
   given the value that was supplied for the last parameter with this name.
   If the value of this last parameter was not supplied by the caller,
   the value of the corresponding property is undefined.


   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=104191

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "10.1.3-1";
var TITLE   = "Variable Instantiation:  Formal Parameters";
var BUGNUMBER="104191";
printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " "+ TITLE);

var myfun1 = new Function( "a", "a", "return a" );
var myfun2 = new Function( "a", "b", "a", "return a" );

function myfun3(a, b, a) {
  return a;
}

// myfun1, myfun2, myfun3 tostring


new TestCase(
  String(myfun2) +"; myfun2(2,4,8)",
  8,
  myfun2(2,4,8) );

new TestCase(
  "myfun2(2,4)",
  void 0,
  myfun2(2,4));

new TestCase(
  String(myfun3) +"; myfun3(2,4,8)",
  8,
  myfun3(2,4,8) );

new TestCase(
  "myfun3(2,4)",
  void 0,
  myfun3(2,4) );

test();

