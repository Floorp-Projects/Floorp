/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          exception-005
 *  ECMA Section:
 *  Description:        Tests for JavaScript Standard Exceptions
 *
 *  ToObject error.
 *
 *  Author:             christine@netscape.com
 *  Date:               31 August 1998
 */
var SECTION = "exception-005";
var TITLE   = "Tests for JavaScript Standard Exceptions: ToObjectError";

writeHeaderToLog( SECTION + " "+ TITLE);

ToObject_1();

test();

function ToObject_1() {
  result = "failed: no exception thrown";
  exception = null;

  try {
    result = foo["bar"];
  } catch ( e ) {
    result = "passed:  threw exception",
      exception = e.toString();
  } finally {
    new TestCase(
      "foo[\"bar\"] [ exception is " + exception +" ]",
      "passed:  threw exception",
      result );
  }
}

