/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465347;
var summary = 'Test integer to id in js_Int32ToId';
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
 
  var o;

  o = new Array();

  expect = undefined;
  o[0xffffffff] = 'end';
  actual = o[-1];  
  reportCompare(expect, actual, summary + ': 1');

  expect = 42;
  o['42'] = 42;
  actual = o[42];  
  reportCompare(expect, actual, summary + ': 2');

  //

  o = new Object();

  expect = undefined;
  o[0xffffffff] = 'end';
  actual = o[-1];  
  reportCompare(expect, actual, summary + ': 3');

  expect = 42;
  o['42'] = 42;
  actual = o[42];  
  reportCompare(expect, actual, summary + ': 4');

  exitFunc ('test');
}
