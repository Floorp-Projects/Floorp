/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465443;
var summary = '';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = 'TypeError: invalid assignment to const \'b\'';


  try
  {
    eval('(function () { const b = 16; var out = []; for (b of [true, "", true, "", true, ""]) out.push(b >> 1) })();');
  }
  catch(ex)
  {
    actual = ex + '';
  }


  reportCompare(expect, actual, summary);
}
