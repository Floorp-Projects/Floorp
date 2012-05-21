// |reftest| fails
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-07-15
 *
 * SUMMARY: Testing Number.prototype.toExponential(fractionDigits)
 * See EMCA 262 Edition 3 Section 15.7.4.6
 *
 * Also see http://bugzilla.mozilla.org/show_bug.cgi?id=90551
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing Number.prototype.toExponential(fractionDigits)';
var cnIsRangeError = 'instanceof RangeError';
var cnNotRangeError = 'NOT instanceof RangeError';
var cnNoErrorCaught = 'NO ERROR CAUGHT...';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var testNum = 77.1234;


status = 'Section A of test: no error intended!';
actual = testNum.toExponential(4);
expect = '7.7123e+1';
captureThis();

status = 'Section B of test: Infinity.toExponential() with out-of-range fractionDigits';
actual = Number.POSITIVE_INFINITY.toExponential(-3);
expect = 'Infinity';
captureThis();

status = 'Section C of test: -Infinity.toExponential() with out-of-range fractionDigits';
actual = Number.NEGATIVE_INFINITY.toExponential(-3);
expect = '-Infinity';
captureThis();

status = 'Section D of test: NaN.toExponential() with out-of-range fractionDigits';
actual = Number.NaN.toExponential(-3);
expect = 'NaN';
captureThis();


///////////////////////////    OOPS....    ///////////////////////////////
/*************************************************************************
 * 15.7.4.6 Number.prototype.toExponential(fractionDigits)
 *
 * An implementation is permitted to extend the behaviour of toExponential
 * for values of fractionDigits less than 0 or greater than 20. In this
 * case toExponential would not necessarily throw RangeError for such values.

status = 'Section B of test: expect RangeError because fractionDigits < 0';
actual = catchError('testNum.toExponential(-4)');
expect = cnIsRangeError;
captureThis();

status = 'Section C of test: expect RangeError because fractionDigits > 20 ';
actual = catchError('testNum.toExponential(21)');
expect = cnIsRangeError;
captureThis();
*************************************************************************/



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function captureThis()
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


function catchError(sEval)
{
  try {eval(sEval);}
  catch(e) {return isRangeError(e);}
  return cnNoErrorCaught;
}


function isRangeError(obj)
{
  if (obj instanceof RangeError)
    return cnIsRangeError;
  return cnNotRangeError;
}
