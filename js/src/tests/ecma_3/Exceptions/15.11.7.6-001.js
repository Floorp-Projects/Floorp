/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    14 April 2003
 * SUMMARY: Prototype of predefined error objects should be DontEnum
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=201989
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 201989;
var summary = 'Prototype of predefined error objects should be DontEnum';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Tests that |F.prototype| is not enumerable in |F|
 */
function testDontEnum(F)
{
  var proto = F.prototype;

  for (var prop in F)
  {
    if (F[prop] === proto)
      return false;
  }
  return true;
}


var list = [
  "Error",
  "ConversionError",
  "EvalError",
  "RangeError",
  "ReferenceError",
  "SyntaxError",
  "TypeError",
  "URIError"
  ];


for (i in list)
{
  var F = this[list[i]];

  // Test for |F|; e.g. Rhino defines |ConversionError| while SM does not.
  if (F)
  {
    status = 'Testing DontEnum attribute of |' + list[i] + '.prototype|';
    actual = testDontEnum(F);
    expect = true;
    addThis();
  }
}



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
