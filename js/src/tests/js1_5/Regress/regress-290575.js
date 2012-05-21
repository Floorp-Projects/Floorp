/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 290575;
var summary = 'Do not crash calling function with more than 32768 arguments';
var actual = 'No Crash';
var expect = 'No Crash';
printBugNumber(BUGNUMBER);
printStatus (summary);

function crashMe(n) {
  var nasty, fn;

  nasty = [];
  while (n--)
    nasty.push("a"+n);   // Function arguments
  nasty.push("void 0");  // Function body
  fn = Function.apply(null, nasty);
  fn.toString();
}

printStatus('crashMe(0x8001)');

crashMe(0x8001);
 
reportCompare(expect, actual, summary);

function crashMe2(n) {
  var nasty = [], fn

    while (n--) nasty[n] = "a"+n
      fn = Function(nasty.join(), "void 0")
      fn.toString()
      }

printStatus('crashMe2(0x10000)');

summary = 'Syntax Error Function to string when more than 65536 arguments';
expect = 'Error';
try
{
  crashMe2(0x10000);
  actual = 'No Error';
  reportCompare(expect, actual, summary);
}
catch(e)
{
  actual = 'Error';
  reportCompare(expect, actual, summary);
  expect = 'SyntaxError';
  actual = e.name;
  reportCompare(expect, actual, summary);
}

