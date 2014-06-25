/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-06-14
 *
 * SUMMARY: Regression test for Bugzilla bug 85880
 *
 * Rhino interpreted mode was nulling out the arguments object of a
 * function if it happened to call another function inside its body.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=85880
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 85880;
var summary = 'Arguments object of g(){f()} should not be null';
var cnNonNull = 'Arguments != null';
var cnNull = 'Arguments == null';
var cnRecurse = true;
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function f1(x)
{
}


function f2()
{
  return f2.arguments;
}
status = 'Section A of test';
actual = (f2() == null);
expect = false;
addThis();

status = 'Section B of test';
actual = (f2(0) == null);
expect = false;
addThis();


function f3()
{
  f1();
  return f3.arguments;
}
status = 'Section C of test';
actual = (f3() == null);
expect = false;
addThis();

status = 'Section D of test';
actual = (f3(0) == null);
expect = false;
addThis();


function f4()
{
  f1();
  f2();
  f3();
  return f4.arguments;
}
status = 'Section E of test';
actual = (f4() == null);
expect = false;
addThis();

status = 'Section F of test';
actual = (f4(0) == null);
expect = false;
addThis();


function f5()
{
  if (cnRecurse)
  {
    cnRecurse = false;
    f5();
  }
  return f5.arguments;
}
status = 'Section G of test';
actual = (f5() == null);
expect = false;
addThis();

status = 'Section H of test';
actual = (f5(0) == null);
expect = false;
addThis();



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = isThisNull(actual);
  expectedvalues[UBound] = isThisNull(expect);
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


function isThisNull(bool)
{
  return bool? cnNull : cnNonNull
    }
