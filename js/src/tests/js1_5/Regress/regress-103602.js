/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    10 Jan 2002
 * SUMMARY: Reassignment to a const is NOT an error per ECMA
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=103602
 *
 * ------- Additional Comment #4 From Brendan Eich 2002-01-10 15:30 -------
 *
 * That's per ECMA (don't blame me, I fought for what Netscape always did:
 * throw an error [could be a catchable exception since 1.3]).
 * Readonly properties, when set by assignment, are not changed, but no error
 * or exception is thrown. The value of the assignment expression is the value
 * of the r.h.s.
 *
 * If you want a *strict* warning, pls change the summary of this bug
 * to say so.
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 103602;
var summary = 'Reassignment to a const is NOT an error per ECMA';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var cnFAIL_1 = 'Redeclaration of a const FAILED to cause an error';
var cnFAIL_2 = 'Reassigning to a const caused an ERROR! It should not!!!';
var sEval = '';

/*
 * Not every implementation supports const (a non-ECMA extension)
 * For example, Rhino does not; it would generate a complile-time error.
 * So we have to hide this so it will be detected at run-time instead.
 */
try
{
  sEval = 'const one = 1';
  eval(sEval);
}
catch(e)
{
  quit(); // if const is not supported, this testcase is over -
}


status = inSection(1);
try
{
  /*
   * Redeclaration of const should be a compile-time error.
   * Hide so it will be detected at run-time.
   */
  sEval = 'const one = 2;';
  eval(sEval);

  expect = ''; // we shouldn't reach this line
  actual = cnFAIL_1;
  addThis();
}
catch(e)
{
  // good - we should be here.
  actual = expect;
  addThis();
}


status = inSection(2);
try
{
  /*
   * Reassignment to a const should be NOT be an error, per ECMA.
   */
  one = 2;
  actual = expect; // good: no error was generated
  addThis();

  // although no error, the assignment should have been ignored -
  status = inSection(3);
  actual = one;
  expect = 1;
  addThis();

  // the value of the EXPRESSION, however, is the value of the r.h.s. -
  status = inSection(4);
  actual = (one = 2);
  expect = 2;
  addThis();
}

catch(e)
{
  // BAD - we shouldn't be here
  expect = '';
  actual = cnFAIL_2;
  addThis();
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
