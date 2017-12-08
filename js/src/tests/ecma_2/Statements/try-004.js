/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          try-004.js
 *  ECMA Section:
 *  Description:        The try statement
 *
 *  This test has a try with one catch block but no finally.
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "try-004";
var TITLE   = "The try statement";

writeHeaderToLog( SECTION + " "+ TITLE);

TryToCatch( "Math.PI", Math.PI );
TryToCatch( "Thrower(5)",   "Caught 5" );
TryToCatch( "Thrower(\"some random exception\")", "Caught some random exception" );

test();

function Thrower( v ) {
  throw "Caught " + v;
}

/**
 *  Evaluate a string.  Catch any exceptions thrown.  If no exception is
 *  expected, verify the result of the evaluation.  If an exception is
 *  expected, verify that we got the right exception.
 */

function TryToCatch( value, expect ) {
  try {
    result = eval( value );
  } catch ( e ) {
    result = e;
  }

  new TestCase(
    "eval( " + value +" )",
    expect,
    result );
}


