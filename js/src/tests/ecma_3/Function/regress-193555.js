/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    17 February 2003
 * SUMMARY: Testing access to function name from inside function
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=193555
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 193555;
var summary = 'Testing access to function name from inside function';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// test via function statement
status = inSection(1);
function f() {return f.toString();};
actual = f();
expect = f.toString();
addThis();

// test via function expression
status = inSection(2);
var x = function g() {return g.toString();};
actual = x();
expect = x.toString();
addThis();

// test via eval() outside function
status = inSection(3);
eval ('function a() {return a.toString();}');
actual = a();
expect = a.toString();
addThis();

status = inSection(4);
eval ('var y = function b() {return b.toString();}');
actual = y();
expect = y.toString();
addThis();

// test via eval() inside function
status = inSection(5);
function c() {return eval('c').toString();};
actual = c();
expect = c.toString();
addThis();

status = inSection(6);
var z = function d() {return eval('d').toString();};
actual = z();
expect = z.toString();
addThis();

// test via two evals!
status = inSection(7);
eval('var w = function e() {return eval("e").toString();}');
actual = w();
expect = w.toString();
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
