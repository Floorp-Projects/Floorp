/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    12 Apr 2002
 * SUMMARY: delete arguments[i] should break connection to local reference
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=137181
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 137181;
var summary = 'delete arguments[i] should break connection to local reference';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
function f1(x)
{
  x = 1;
  delete arguments[0];
  return x;
}
actual = f1(0); // (bug: Rhino was returning |undefined|)
expect = 1;
addThis();


status = inSection(2);
function f2(x)
{
  x = 1;
  delete arguments[0];
  arguments[0] = -1;
  return x;
}
actual = f2(0); // (bug: Rhino was returning -1)
expect = 1;
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
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
