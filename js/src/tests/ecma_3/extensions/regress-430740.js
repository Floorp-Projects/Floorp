/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 430740;
var summary = 'Do not strip format-control characters from string literals';
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
 
  function doevil() {
    print('evildone');
    return 'evildone';
  }

  expect = 'a%E2%80%8D,+doevil()%5D)//';
  actual += eval("(['a\\\u200d', '+doevil()])//'])");
  actual = encodeURI(actual);
  reportCompare(expect, actual, summary);

  expect = 'a%EF%BF%BE,+doevil()%5D)//';
  actual = eval("(['a\\\ufffe', '+doevil()])//'])"); 
  actual = encodeURI(actual);
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
