/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          regress-7224.js
 *  Reference:          js1_2
 *  Description:        Remove support for the arg
 *  Author:             ** replace with your e-mail address **
 */

var SECTION = "regress";       // provide a document reference (ie, ECMA section)
var VERSION = "JS1_4"; // Version of JavaScript or ECMA
var TITLE   = "Regression test for bugzilla #7224";       // Provide ECMA section title or a description
var BUGNUMBER = "http://bugzilla.mozilla.org/show_bug.cgi?id=7224";     // Provide URL to bugsplat or bugzilla report

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

var f = new Function( "return arguments.caller" );
var o = {};

o.foo = f;
o.foo("a", "b", "c");


AddTestCase(
  "var f = new Function( 'return arguments.caller' ); f()",
  undefined,
  f() );

AddTestCase(
  "var o = {}; o.foo = f; o.foo('a')",
  undefined,
  o.foo('a') );

test();       // leave this alone.  this executes the test cases and
// displays results.
