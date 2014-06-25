/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-------------------------------------------------------------------------------------------------
var BUGNUMBER = '58946';
var stat =  'Testing a return statement inside a catch statement inside a function';

test();

function test() {
  enterFunc ("test");
  printBugNumber(BUGNUMBER);
  printStatus (stat);

  expect = 'PASS';

  function f()
  {
    try
    {
      throw 'PASS';
    }
    catch(e)
    {
      return e;
    }
  }

  actual = f();

  reportCompare(expect, actual, stat);

  exitFunc ("test");
}
