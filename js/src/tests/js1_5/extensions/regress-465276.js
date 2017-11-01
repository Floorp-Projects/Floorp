/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465276;
var summary = '((1 * (1))|""';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  expect = '[1, 1, 1, 1, 1, 1, 1, 1, 1, 1]';
  empty = [];
  out = [];
  for (var j=0;j<10;++j) { empty[42]; out.push((1 * (1)) | ""); }
  print(actual = uneval(out));


  reportCompare(expect, actual, summary);
}
