// |reftest| fails
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-07-15
 *
 * SUMMARY: Testing Number.prototype.toPrecision(precision)
 * See EMCA 262 Edition 3 Section 15.7.4.7
 *
 * Also see http://bugzilla.mozilla.org/show_bug.cgi?id=90551
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing Number.prototype.toPrecision(precision)';
var cnIsRangeError = 'instanceof RangeError';
var cnNotRangeError = 'NOT instanceof RangeError';
var cnNoErrorCaught = 'NO ERROR CAUGHT...';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var testNum = 5.123456;


status = 'Section A of test: no error intended!';
actual = testNum.toPrecision(4);
expect = '5.123';
captureThis();

status = 'Section B of test: Infinity.toPrecision() with out-of-range fractionDigits';
actual = Number.POSITIVE_INFINITY.toPrecision(-3);
expect = 'Infinity';
captureThis();

status = 'Section C of test: -Infinity.toPrecision() with out-of-range fractionDigits';
actual = Number.NEGATIVE_INFINITY.toPrecision(-3);
expect = '-Infinity';
captureThis();

status = 'Section D of test: NaN.toPrecision() with out-of-range fractionDigits';
actual = Number.NaN.toPrecision(-3);
expect = 'NaN';
captureThis();


///////////////////////////    OOPS....    ///////////////////////////////
/*************************************************************************
 * 15.7.4.7 Number.prototype.toPrecision(precision)
 *
 * An implementation is permitted to extend the behaviour of toPrecision
 * for values of precision less than 1 or greater than 21. In this
 * case toPrecision would not necessarily throw RangeError for such values.

status = 'Section B of test: expect RangeError because precision < 1';
actual = catchError('testNum.toPrecision(0)');
expect = cnIsRangeError;
captureThis();

status = 'Section C of test: expect RangeError because precision < 1';
actual = catchError('testNum.toPrecision(-4)');
expect = cnIsRangeError;
captureThis();

status = 'Section D of test: expect RangeError because precision > 21 ';
actual = catchError('testNum.toPrecision(22)');
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
