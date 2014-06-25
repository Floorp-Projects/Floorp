/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 480096;
var summary = 'Array.lastIndexOf';
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

  expect = '-12';
  actual = 0;
  actual += Array.lastIndexOf([2, 3,, 4, 5, 6]);
  actual += [2, 3,, 4, 5, 6].lastIndexOf();
  actual += Array.prototype.lastIndexOf.call([2, 3,, 4, 5, 6]);
  actual += Array.prototype.lastIndexOf.apply([2, 3,, 4, 5, 6], [, -4]);
  actual += Array.prototype.lastIndexOf.apply([2, 3,, 4, 5, 6], [undefined, -4]);
  actual += Array.prototype.lastIndexOf.apply([2, 3,, 4, 5, 6], [undefined, -5]);
  actual += Array.lastIndexOf([2, 3,, 4, 5, 6], undefined);
  actual += Array.lastIndexOf([2, 3,, 4, 5, 6], undefined, 1);
  actual += Array.lastIndexOf([2, 3,, 4, 5, 6], undefined, 2);
  actual += Array.lastIndexOf([2, 3,, 4, 5, 6], undefined);
  actual += Array.lastIndexOf([2, 3,, 4, 5, 6], undefined, 1);
  actual += Array.lastIndexOf([2, 3,, 4, 5, 6], undefined, 2);

  actual = String(actual);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
