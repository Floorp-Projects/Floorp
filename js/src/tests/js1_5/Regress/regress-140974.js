/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    04 May 2002
 * SUMMARY: |if (false) {var x;} should create the variable x
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=140974
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 140974;
var TEST_PASSED = 'variable was created';
var TEST_FAILED = 'variable was NOT created';
var summary = '|if (false) {var x;}| should create the variable x';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// --------------  THESE TWO SECTIONS TEST THE VARIABLE X  --------------
status = inSection(1);
actual = TEST_PASSED;
try{ X;} catch(e) {actual = TEST_FAILED}
expect = TEST_PASSED;
addThis();

var X;

status = inSection(2);
actual = TEST_PASSED;
try{ X;} catch(e) {actual = TEST_FAILED}
expect = TEST_PASSED;
addThis();



// --------------  THESE TWO SECTIONS TEST THE VARIABLE Y  --------------
status = inSection(3);
actual = TEST_PASSED;
try{ Y;} catch(e) {actual = TEST_FAILED}
expect = TEST_PASSED;
addThis();

if (false) {var Y;};

status = inSection(4);
actual = TEST_PASSED;
try{ Y;} catch(e) {actual = TEST_FAILED}
expect = TEST_PASSED;
addThis();



// --------------  THESE TWO SECTIONS TEST THE VARIABLE Z  --------------
status = inSection(5);
actual = TEST_PASSED;
try{ Z;} catch(e) {actual = TEST_FAILED}
expect = TEST_PASSED;
addThis();

if (false) { for (var Z; false;){} }

status = inSection(6);
actual = TEST_PASSED;
try{ Z;} catch(e) {actual = TEST_FAILED}
expect = TEST_PASSED;
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
