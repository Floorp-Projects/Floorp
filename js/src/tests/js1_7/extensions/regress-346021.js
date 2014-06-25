/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346021;
var summary = 'Implementing __iterator__ as generator';
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
 
  var o = { __iterator__: function () { print(12); yield 42; } };

  expect = 42;
  actual = 0;
 
  for (let i in Iterator(o))
  {
    actual = i;
  }

  reportCompare(expect, actual, summary);

  actual = 0;

  for (let i in o)
  {
    actual = i; // this doesn't iterate 42
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
