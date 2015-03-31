/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 345736;
var summary = 'for each in array comprehensions';
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

  var arr;

  arr=[x+x for (x in ["a","b","c"])];

  expect = '00,11,22';
  actual = arr.toString();
  reportCompare(expect, actual, summary);

  arr=[x+x for each (x in ["a","b","c"])];

  expect = 'aa,bb,cc';
  actual = arr.toString();
  reportCompare(expect, actual, summary);

  // Before JS1.7's destructuring for…in was fixed to match JS1.8's,
  // the expected result was 'aa,bb,cc'.
  arr=[x+x for ([,x] in ["a","b","c"])];
  expect = 'NaN,NaN,NaN';
  actual = arr.toString();
  reportCompare(expect, actual, summary);

  // Before JS1.7's destructuring for…in was fixed to match JS1.8's,
  // the expected result was '0a,1b,2c'.
  arr=[x+y for ([x,y] in ["a","b","c"])];
  expect = '0undefined,1undefined,2undefined';
  actual = arr.toString();
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
