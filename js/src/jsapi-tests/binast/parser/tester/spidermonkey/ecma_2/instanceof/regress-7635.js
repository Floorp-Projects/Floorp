/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          regress-7635.js
 *  Reference:          http://bugzilla.mozilla.org/show_bug.cgi?id=7635
 *  Description:        instanceof tweaks
 *  Author:            
 */

var SECTION = "instanceof";       // provide a document reference (ie, ECMA section)
var VERSION = "ECMA_2"; // Version of JavaScript or ECMA
var TITLE   = "Regression test for Bugzilla #7635";       // Provide ECMA section title or a description
var BUGNUMBER = "7635";     // Provide URL to bugsplat or bugzilla report

startTest();               // leave this alone

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

function Foo() {}
theproto = {};
Foo.prototype = theproto
  theproto instanceof Foo


  AddTestCase( "function Foo() {}; theproto = {}; Foo.prototype = theproto; theproto instanceof Foo",
	       false,
	       theproto instanceof Foo );

var f = new Function();

AddTestCase( "var f = new Function(); f instanceof f", false, f instanceof f );


test();       // leave this alone.  this executes the test cases and
// displays results.
