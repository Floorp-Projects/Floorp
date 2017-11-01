/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    14 April 2003
 * SUMMARY: Prototype of predefined error objects should be DontDelete
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=201989
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 201989;
var summary = 'Prototype of predefined error objects should be DontDelete';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Tests that |F.prototype| is DontDelete
 */
function testDontDelete(F)
{
  var e;
  var orig = F.prototype;
  try
  {
    delete F.prototype;
  }
  catch (e)
  {
  }
  return F.prototype === orig;
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
    status = 'Testing DontDelete attribute of |' + list[i] + '.prototype|';
    actual = testDontDelete(F);
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
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
