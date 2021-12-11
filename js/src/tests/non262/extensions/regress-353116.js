/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 353116;
var summary = 'Improve errors messages for null, undefined properties';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = /TypeError: (undefined has no properties|can't access property "y" of undefined)/;
  actual = 'No Error';

  try
  {
    undefined.y;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);

  expect = /TypeError: (null has no properties|can't access property "y" of null)/;
  actual = 'No Error';

  try
  {
    null.y;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);

  expect = /TypeError: .*x is undefined/;
  actual = 'No Error';

  try
  {
    x = undefined; 
    x.y;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);

  expect = /TypeError: .*x is null/;
  actual = 'No Error';

  try
  {
    x = null; 
    x.y;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);
}
