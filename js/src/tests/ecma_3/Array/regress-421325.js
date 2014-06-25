/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 421325;
var summary = 'Dense Arrays and holes';
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
 
  Array.prototype[1] = 'bar';

  var a = []; 
  a[0]='foo'; 
  a[2] = 'baz'; 
  expect = 'foo,bar,baz';
  actual = a + '';

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
