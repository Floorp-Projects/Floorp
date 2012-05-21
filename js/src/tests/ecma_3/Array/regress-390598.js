/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 390598;
var summary = 'Override inherited length of Array-like object';
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
 

  function F() {} 
  F.prototype = []; 

  // override inherited length from the prototype.
  expect = 10;
  var x = new F(); 

  print('x = new F(); x instanceof Array: ' + (x instanceof Array));

  x.length = expect;
  actual = x.length;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
