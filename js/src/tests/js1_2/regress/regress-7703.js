/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */



/**
 *  File Name:          regress-7703.js
 *  Reference:          "http://bugzilla.mozilla.org/show_bug.cgi?id=7703";
 *  Description:        See the text of the bugnumber above
 */

var SECTION = "js1_2";       // provide a document reference (ie, ECMA section)
var TITLE   = "Regression test for bugzilla # 7703";       // Provide ECMA section title or a description
var BUGNUMBER = "http://bugzilla.mozilla.org/show_bug.cgi?id=7703";     // Provide URL to bugsplat or bugzilla report

printBugNumber(BUGNUMBER);

/*
 * Calls to AddTestCase here. AddTestCase is a function that is defined
 * in shell.js and takes three arguments:
 * - a string representation of what is being tested
 * - the expected result
 * - the actual result
 *
 * For example, a test might look like this:
 *
 * var zip = /[\d]{5}$/;
 *
 * AddTestCase(
 * "zip = /[\d]{5}$/; \"PO Box 12345 Boston, MA 02134\".match(zip)",   // description of the test
 *  "02134",                                                           // expected result
 *  "PO Box 12345 Boston, MA 02134".match(zip) );                      // actual result
 *
 */

types = [];
function inspect(object) {
  for (prop in object) {
    var x = object[prop];
    types[types.length] = (typeof x);
  }
}

var o = {a: 1, b: 2};
inspect(o);

AddTestCase( "inspect(o),length",   2,       types.length );
AddTestCase( "inspect(o)[0]",      "number", types[0] );
AddTestCase( "inspect(o)[1]",      "number", types[1] );

types_2 = [];

function inspect_again(object) {
  for (prop in object) {
    types_2[types_2.length] = (typeof object[prop]);
  }
}

inspect_again(o);
AddTestCase( "inspect_again(o),length",   2,       types.length );
AddTestCase( "inspect_again(o)[0]",      "number", types[0] );
AddTestCase( "inspect_again(o)[1]",      "number", types[1] );


test();       // leave this alone.  this executes the test cases and
// displays results.
