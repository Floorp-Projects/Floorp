// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    30 Jan 2002
 * Revised: 10 Apr 2002
 * Revised: 14 July 2002
 *
 * SUMMARY: JS should error on |for(i in undefined)|, |for(i in null)|
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=121744
 *
 * ECMA-262 3rd Edition Final spec says such statements should error. See:
 *
 *               Section 12.6.4  The for-in Statement
 *               Section 9.9     ToObject
 *
 *
 * BUT: SpiderMonkey has decided NOT to follow this; it's a bug in the spec.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=131348
 *
 * UPDATE: Rhino has also decided not to follow the spec on this.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=136893
 *

 |--------------------------------------------------------------------|
 |                                                                    |
 | So for now, adding an early return for this test so it won't run.  |
 |                                                                    |
 |--------------------------------------------------------------------|

 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 121744;
var summary = 'JS should error on |for(i in undefined)|, |for(i in null)|';
var TEST_PASSED = 'TypeError';
var TEST_FAILED = 'Generated an error, but NOT a TypeError!';
var TEST_FAILED_BADLY = 'Did not generate ANY error!!!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

/*
 * AS OF 14 JULY 2002, DON'T RUN THIS TEST IN EITHER RHINO OR SPIDERMONKEY -
 */
quit();


status = inSection(1);
expect = TEST_PASSED;
actual = TEST_FAILED_BADLY;
/*
 * OK, this should generate a TypeError
 */
try
{
  for (var i in undefined)
  {
    print(i);
  }
}
catch(e)
{
  if (e instanceof TypeError)
    actual = TEST_PASSED;
  else
    actual = TEST_FAILED;
}
addThis();



status = inSection(2);
expect = TEST_PASSED;
actual = TEST_FAILED_BADLY;
/*
 * OK, this should generate a TypeError
 */
try
{
  for (var i in null)
  {
    print(i);
  }
}
catch(e)
{
  if (e instanceof TypeError)
    actual = TEST_PASSED;
  else
    actual = TEST_FAILED;
}
addThis();



status = inSection(3);
expect = TEST_PASSED;
actual = TEST_FAILED_BADLY;
/*
 * Variable names that cannot be looked up generate ReferenceErrors; however,
 * property names like obj.ZZZ that cannot be looked up are set to |undefined|
 *
 * Therefore, this should indirectly test | for (var i in undefined) |
 */
try
{
  for (var i in this.ZZZ)
  {
    print(i);
  }
}
catch(e)
{
  if(e instanceof TypeError)
    actual = TEST_PASSED;
  else
    actual = TEST_FAILED;
}
addThis();



status = inSection(4);
expect = TEST_PASSED;
actual = TEST_FAILED_BADLY;
/*
 * The result of an unsuccessful regexp match is the null value
 * Therefore, this should indirectly test | for (var i in null) |
 */
try
{
  for (var i in 'bbb'.match(/aaa/))
  {
    print(i);
  }
}
catch(e)
{
  if(e instanceof TypeError)
    actual = TEST_PASSED;
  else
    actual = TEST_FAILED;
}
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
