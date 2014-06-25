/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
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

// ------- Comment #110 From Brendan Eich

// (In reply to comment #107)

  function f(a) { const b = a; print(++b); return b; }

  expect = 'function f(a) { const b = a; print(++b); return b; }';
  actual = f + '';
  compareSource(expect, actual, 'function f(a) { const b = a; print(++b); return b; }');

  expect = '01';
  actual = 0;

  function g(a) { const b = a; print(actual = ++b); return b; }
  actual = String(actual) + g(1);
  reportCompare(expect, actual, 'function g(a) { const b = a; print(actual = ++b); return b; }');

  expect = '21';
  actual = 0;

  const x = 1; print(actual = ++x);
  actual = String(actual) + x;

  reportCompare(expect, actual, 'const x = 1; print(actual = ++x); ');

  exitFunc ('test');
}
