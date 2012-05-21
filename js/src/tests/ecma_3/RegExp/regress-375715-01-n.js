/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 375715;
var summary = 'Do not assert: (c2 <= cs->length) && (c1 <= c2)';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  // note that the assertion does not fire if the regexp is
  // evald or used in new RegExp, so this test must be an -n 
  // with uncaught SyntaxError.

  /[\Wb-G]/.exec("");
  reportCompare(expect, actual, summary + ' /[\Wb-G]/.exec("")');

  exitFunc ('test');
}
