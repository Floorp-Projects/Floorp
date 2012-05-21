/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351496;
var summary = 'decompilation of case let (y = 3) expression';
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

  var f = function() { switch(0) { case let(y = 3) 6: } }
  actual = f + '';
  expect = 'function () {\n' +
    '    switch (0) {\n' +
    '      case let (y = 3) 6:\n' +
    '      default:;\n' +
    '    }\n' +
    '}';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
