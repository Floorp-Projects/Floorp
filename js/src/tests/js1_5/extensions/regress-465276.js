/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  jit(true);

  expect = '[1, 1, 1, 1, 1, 1, 1, 1, 1, 1]';
  empty = [];
  out = [];
  for (var j=0;j<10;++j) { empty[42]; out.push((1 * (1)) | ""); }
  print(actual = uneval(out));

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
