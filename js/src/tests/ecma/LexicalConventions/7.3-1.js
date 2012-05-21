/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.3-1.js
   ECMA Section:       7.3 Comments
   Description:


   Author:             christine@netscape.com
   Date:               12 november 1997

*/
var SECTION = "7.3-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Comments";

writeHeaderToLog( SECTION + " "+ TITLE);

var testcase;

testcase = new TestCase( SECTION,
			 "a comment with a line terminator string, and text following",
			 "pass",
			 "pass");

// "\u000A" testcase.actual = "fail";


testcase = new TestCase( SECTION,
			 "// test \\n testcase.actual = \"pass\"",
			 "pass",
			 "" );

var x = "// test \n testcase.actual = 'pass'";

testcase.actual = eval(x);

test();

// XXX bc replace test()
function test() {
  for ( gTc=0; gTc < gTestcases.length; gTc++ ) {
    gTestcases[gTc].passed = writeTestCaseResult(
      gTestcases[gTc].expect,
      gTestcases[gTc].actual,
      gTestcases[gTc].description +":  "+
      gTestcases[gTc].actual );

    gTestcases[gTc].reason += ( gTestcases[gTc].passed ) ? "" : " ignored chars after line terminator of single-line comment";
  }
  stopTest();
  return ( gTestcases );
}
