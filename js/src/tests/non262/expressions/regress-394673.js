/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 394673;
var summary = 'Parsing long chains of "&&" or "||"';
var actual = 'No Error';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var N = 70*1000;
var counter;

counter = 0;
var x = build("&&")();
if (x !== true)
  throw "Unexpected result: x="+x;
if (counter != N)
  throw "Unexpected counter: counter="+counter;

counter = 0;
var x = build("||")();
if (x !== true)
  throw "Unexpected result: x="+x;
if (counter != 1)
  throw "Unexpected counter: counter="+counter;

function build(operation)
{
  var counter;
  var a = [];
  a.push("return f()");
  for (var i = 1; i != N - 1; ++i)
    a.push("f()");
  a.push("f();");
  return new Function(a.join(operation));
}

function f()
{
  ++counter;
  return true;
}

reportCompare(expect, actual, summary);
