/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355497;
var summary = 'Do not overflow stack with Array.slice, getter';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = 'InternalError: too much recursion';

  try
  {
    var a = { length: 1 };
    a.__defineGetter__(0, [].slice);
    a[0];
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 1');

  try
  {
    var b = { length: 1 };
    b.__defineGetter__(0, function () { return Array.prototype.slice.call(b); });
    b[0];
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 2');

  try
  {
    var c = [];
    c.__defineSetter__(0, c.unshift);
    c[0] = 1;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 3');
}
