/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 380237;
var summary = 'Decompilation of generator expressions';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

expect = 'SyntaxError: yield not in function';
actual = '';
try
{
  eval('((yield i) for (i in [1,2,3]))');
}
catch(ex)
{
  actual = ex + '';
}
reportCompare(expect, actual, summary + ': top level');


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var f = (function() { g = (d for (d in [0]) for (e in [1])); });
  expect = 'function() { g = (d for (d in [0]) for (e in [1])); }';
  actual = f + '';
  compareSource(expect, actual, summary + ': see bug 380506');

  f = function() { return (1 for (i in [])) };
  expect = 'function() { return (1 for (i in [])); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { with((x for (x in []))) { } };
  expect = 'function() { with(x for (x in [])) { } }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { (1 for (w in []) if (false)) });
  expect = 'function() { (1 for (w in []) if (false)); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { (1 for (w in []) if (x)) });
  expect = 'function() { (1 for (w in []) if (x)); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { (1 for (w in []) if (1)) });
  expect = 'function() { (1 for (w in []) ); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = (function() { (x for ([{}, {}] in [])); });
  expect = 'function() { (x for ([[], []] in [])); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = 'ReferenceError: invalid assignment left-hand side';
  actual = '';
  try
  {
    eval('(function() { (x for each (x in [])) = 3; })');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': Do not Assert: *pc == JSOP_CALL');

  f = (function() { (x*x for (x in a)); });
  expect = 'function() { (x*x for (x in a)); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = 'SyntaxError: illegal use of yield in generator expression';
  actual = '';
  try
  {
    eval('(function () { (1 for (y in (yield 3))); })');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);
   
  expect = 'ReferenceError: invalid delete operand';
  try
  {
    eval('(function () { delete (x for (x in [])); })');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': Do not Assert: *pc == JSOP_CALL');

  expect = 'SyntaxError: illegal use of yield in generator expression';
  actual = '';
  try
  {
    eval('(function() { ([yield] for (x in [])); })');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  f = function() { if(1, (x for (x in []))) { } };
  expect = 'function() { if(1, (x for (x in []))) { } }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function () {return(i*j for each (i in [1,2,3,4]) 
                          for each (j in [5,6,7,8]))};
  expect = 'function () {return(i*j for each (i in [1,2,3,4]) ' + 
    'for each (j in [5,6,7,8]));}';
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = 'SyntaxError: yield not in function';
  actual = '';
  try
  {
    eval('((yield i) for (i in [1,2,3]))');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  f = function() { try { } catch(x if (1 for (x in []))) { } finally { } };
  expect = 'function() { try {} catch(x if (1 for (x in []))) {} finally {} }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = eval(uneval(f));
  expect = 'function() { try {} catch(x if (1 for (x in []))) {} finally {} }';
  actual = f + '';
  compareSource(expect, actual, summary + ': eval(uneval())');

  f = function() { if (1, (x for (x in []))) { } };
  expect = 'function() { if (1, (x for (x in []))) { } }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { ((a, b) for (x in [])) };
  expect = 'function() { ((a, b) for (x in [])); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
