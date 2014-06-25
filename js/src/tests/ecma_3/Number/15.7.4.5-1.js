/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-07-15
 *
 * SUMMARY: Testing Number.prototype.toFixed(fractionDigits)
 * See EMCA 262 Edition 3 Section 15.7.4.5
 *
 * Also see http://bugzilla.mozilla.org/show_bug.cgi?id=90551
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing Number.prototype.toFixed(fractionDigits)';
var cnIsRangeError = 'instanceof RangeError';
var cnNotRangeError = 'NOT instanceof RangeError';
var cnNoErrorCaught = 'NO ERROR CAUGHT...';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var testNum = 234.2040506;


status = 'Section A of test: no error intended!';
actual = testNum.toFixed(4);
expect = '234.2041';
captureThis();


///////////////////////////    OOPS....    ///////////////////////////////
/*************************************************************************
 * 15.7.4.5 Number.prototype.toFixed(fractionDigits)
 *
 * An implementation is permitted to extend the behaviour of toFixed
 * for values of fractionDigits less than 0 or greater than 20. In this
 * case toFixed would not necessarily throw RangeError for such values.

status = 'Section B of test: expect RangeError because fractionDigits < 0';
actual = catchError('testNum.toFixed(-4)');
expect = cnIsRangeError;
captureThis();

status = 'Section C of test: expect RangeError because fractionDigits > 20 ';
actual = catchError('testNum.toFixed(21)');
expect = cnIsRangeError;
captureThis();
*************************************************************************/


status = 'Section D of test: no error intended!';
actual =  0.00001.toFixed(2);
expect = '0.00';
captureThis();

status = 'Section E of test: no error intended!';
actual =  0.000000000000000000001.toFixed(20);
expect = '0.00000000000000000000';
captureThis();



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
