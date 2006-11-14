/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an
* "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 2001-07-15
*
* SUMMARY: Testing Number.prototype.toPrecision(precision)
* See EMCA 262 Edition 3 Section 15.7.4.7
*
*/
//-----------------------------------------------------------------------------
    var SECTION = "15.7.4.6-1";
    var VERSION = "";
    var TITLE = "Testing Number.prototype.toPrecision(precision)";
    var bug = '(none)';
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " " + TITLE);
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    
    var UBound = 0;
    var cnIsRangeError = 'instanceof RangeError';
    var cnNotRangeError = 'NOT instanceof RangeError';
    var cnNoErrorCaught = 'NO ERROR CAUGHT...';
    var status = '';
    var statusitems = [];
    var actual = '';
    var actualvalues = [];
    var expect= '';
    var expectedvalues = [];
    var testNum:Number = 5.123456;
    var testNum2 = NaN
    var testNum3 = Number.POSITIVE_INFINITY
    var testNum4 = 0;
    var testNum5 = 4000;


    status = 'Section A of test: no error intended!';
    actual = testNum.toPrecision(4);
    expect = '5.123';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);


    status = 'Section B of test: no error intended!';
    actual = testNum.toPrecision(undefined);
    expect = testNum.toString();
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);




    status = 'Section C of test: no error intended!';
    actual = Number.prototype.toPrecision.length
    expect = 1
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section D of test: no error intended!';
    actual = testNum2.toPrecision(6);
    expect = "NaN"
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section E of test: no error intended!';
    actual = testNum3.toPrecision(6);
    expect = "Infinity"
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    var thisError:String="no error";
    try{
        testNum.toPrecision(0)
    }catch(e:RangeError){
        thisError=e.toString();
    }

    status = 'Section F of test: error intended!';
    actual = rangeError(thisError);
    expect = "RangeError: Error #1002"
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);


    status = 'Section G of test: no error intended!';
    actual = testNum.toPrecision(21);
    expect = "5.12345600000000001017"
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    var thisError:String="no error";
    try{
        testNum.toPrecision(-1)
    }catch(e:RangeError){
        thisError=e.toString();
    }

    status = 'Section H of test: error intended!';
    actual = rangeError(thisError);
    expect = "RangeError: Error #1002"
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    var thisError:String="no error";
    try{
        testNum.toPrecision(22)
    }catch(e:RangeError){
        thisError=e.toString();
    }

    status = 'Section I of test: error intended!';
    actual = rangeError(thisError);
    expect = "RangeError: Error #1002"
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section J of test: no error intended!';
    actual = testNum4.toPrecision(4);
    expect = '0.0000';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section K of test: no error intended!';
    actual = testNum5.toPrecision(3);
    expect = '4.00e+3';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);


///////////////////////////    OOPS....    ///////////////////////////////

    return array;
}

/*
function captureThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}
*/

/*
function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
 
  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
*/

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
