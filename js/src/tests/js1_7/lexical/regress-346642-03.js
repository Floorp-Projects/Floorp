// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346642;
var summary = 'decompilation of destructuring assignment';
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

  expect = 'TypeError: NaN is not a constructor';
  actual = 'No Crash';
  try
  {
    try { throw 1; } catch(e1 if 0) { } catch(e2 if (new NaN)) { }
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 1');

  expect = /TypeError: x.t (has no properties|is undefined)/;
  actual = 'No Crash';
  try
  {
    z = [1];
    let (x = (undefined ? 3 : z)) { x.t.g }
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary + ': 2');

  expect = /TypeError: x.t (has no properties|is undefined)/;
  actual = 'No Crash';
  try
  {
    z = [1];
    new Function("let (x = (undefined ? 3 : z)) { x.t.g }")()
      }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary + ': 3');

  expect = 'TypeError: b is not a constructor';
  actual = 'No Crash';
  try
  {
    with({x: (new (b = 1))}) (2).x
      }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 4');

  expect = /TypeError: this.zzz (has no properties|is undefined)/;
  actual = 'No Crash';
  try
  {
    (function(){try { } catch(f) { return; } finally { this.zzz.zzz }})();
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary + ': 5');

  expect = 'TypeError: p.z = <x><y/></x> ? 3 : 4 is not a function';
  actual = 'No Crash';
  try
  {
    (new Function("if(\n({y:5, p: (print).r})) { p={}; (p.z = <x\n><y/></x> ? 3 : 4)(5) }"))();
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 6');

  expect = 'TypeError: xx is not a function';
  actual = 'No Crash';
  try
  {
    switch(xx) { case 3: case (new ([3].map)): } const xx;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 7');

  expect = 'ReferenceError: x.y is not defined';
  actual = 'No Crash';
  try
  {
    x = {};
    import x.y;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 9');

  exitFunc ('test');
}
