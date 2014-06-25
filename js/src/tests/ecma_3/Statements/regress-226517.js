/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    04 Dec 2003
 * SUMMARY: |finally| statement should execute even after a |return|
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=226517
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 226517;
var summary = '|finally| statement should execute even after a |return|';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * We can only use a return statement within a function.
 * That makes the testcase more complicated to set up -
 */
function f()
{
  var return_expression_was_calculated = false;
  try
  {
    return (return_expression_was_calculated = true);
  }
  finally
  {
    actual = return_expression_was_calculated;
  }
}


status = inSection(1);
f(); // sets |actual|
expect = true;
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
