/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    4 Oct 2010
 * SUMMARY: Error instances have no special properties beyond those inherited
 *          from the Error prototype object
 */
//-----------------------------------------------------------------------------
var summary = 'Error instances have no special properties beyond those inherited the Error prototype object';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printStatus (summary);

  var actual = '';
  var expect = 'TypeError: Error.prototype is not a constructor';
  try {
      new Error.prototype;
  } catch (e) {
      actual = '' + e;
  }

  reportCompare(actual, expect, "not a constructor");

  exitFunc ('test');
}
