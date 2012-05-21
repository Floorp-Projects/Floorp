/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355583;
var summary = 'block object access to arbitrary stack slots';
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

  expect = 'No Crash';
  actual = 'No Crash';
  try
  {
    (function() {
      let b = parent(function(){});
      print(b[1] = throwError);
    })();
  }
  catch(ex)
  {

  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
