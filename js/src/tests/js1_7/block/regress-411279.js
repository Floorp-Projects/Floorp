/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 411279;
var summary = 'let declaration as direct child of switch body block';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function f(x) 
  {
    var value = '';

    switch(x)
    {
    case 1:
      value = "1 " + y; 
      break; 
    case 2: 
      let y = 42;
      value = "2 " + y; 
      break; 
    default:
      value = "default " + y;
    }

    return value;
  }

  expect = '1 undefined';
  actual = f(1);
  reportCompare(expect, actual, summary + ': f(1)');

  expect = '2 42';
  actual = f(2);
  reportCompare(expect, actual, summary + ': f(2)');

  expect = 'default undefined';
  actual = f(3);
  reportCompare(expect, actual, summary + ': f(3)');
 
  exitFunc ('test');
}
