/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 383269;
var summary = 'Allow override of arguments';
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
 
  var expect1 = '33,42';
  var expect2 = 33;
  var actual1 = '';
  var actual2 = '';

  function f(){
    var a=arguments; actual1 = a[0]; arguments=42; actual1 += ',' + arguments; return a;
  }

  actual2 = f(33)[0];

  expect = expect1 + ':' + expect2;
  actual = actual1 + ':' + actual2;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
