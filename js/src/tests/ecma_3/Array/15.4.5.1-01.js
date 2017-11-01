/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var summary = '15.4.5.1 - array.length coverage';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printStatus (summary);

  var a = [];
 
  expect = 'RangeError: invalid array length';
  actual = '';
  try
  {
    a.length = -1;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  actual = '';
  try
  {
    a.length = 12345678901234567890;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  actual = '';
  try
  {
    a.length = 'a';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);
}
