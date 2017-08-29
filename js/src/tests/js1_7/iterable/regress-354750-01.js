/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 354750;
var summary = 'Changing Iterator.prototype.next should not affect default iterator';
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
 
  Iterator.prototype.next = function() {
    throw "This should not be thrown";
  }

  expect = 'No exception';
  actual = 'No exception';
  try
  {
    for (var i in [])
    {
    }
  }
  catch(ex)
  {
    actual = ex;
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
