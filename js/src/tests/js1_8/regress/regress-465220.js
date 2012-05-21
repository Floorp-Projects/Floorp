/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465220;
var summary = 'Do not assert: anti-nesting';
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

  expect = 'TypeError: can\'t convert o to primitive type';

  jit(true);
 
  try
  {
    var o = {toString: function()(i > 2) ? this : "foo"};
    var s = "";
    for (var i = 0; i < 5; i++)
      s += o + o;
    print(s);
    actual = 'No Exception';
  }
  catch(ex)
  {
    actual = 'TypeError: can\'t convert o to primitive type';
  }
  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
