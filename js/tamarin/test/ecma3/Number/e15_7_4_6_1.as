/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */
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
* SUMMARY: Testing Number.prototype.toExponential(fractionDigits)
* See EMCA 262 Edition 3 Section 15.7.4.6
*
*/
//-----------------------------------------------------------------------------
    var SECTION = "15.7.4.6-1";
    var VERSION = "";
    var TITLE = "Testing Number.prototype.toExponential(fractionDigits)";
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
    var testNum = 77.1234;
    var testNum2=NaN;
    var testNum3=0;
    var testNum4=Number.POSITIVE_INFINITY;
    var testNum5=315003;


    status = 'Section A of test: no error intended!';
    actual = testNum.toExponential(4);
    expect = '7.7123e+1';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section B of test: no error intended!';
    actual = Number.prototype.toExponential.length
    expect = 1
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section C of test: no error intended!';
    actual = testNum.toExponential();
    expect = '7.71234e+1';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section D of test: no error intended!';
    actual = testNum.toExponential(5);
    expect = '7.71234e+1';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section E of test: no error intended!';
    actual = testNum2.toExponential(5);
    expect = 'NaN';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section F of test: no error intended!';
    actual = testNum3.toExponential(5);
    expect = '0.00000e-16';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section G of test: no error intended!';
    actual = testNum4.toExponential(4);
    expect = 'Infinity';
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    var thisError:String="no error";
    try{
        testNum.toExponential(-1);
    }catch(e:RangeError){
        thisError=e.toString()
    }

    status = 'Section H of test: error intended!';
    actual = rangeError(thisError);
    expect = "RangeError: Error #1002";
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    var thisError:String="no error";
    try{
        testNum.toExponential(21);
    }catch(e:RangeError){
        thisError=e.toString()
    }

    status = 'Section I of test: error intended!';
    actual = rangeError(thisError);
    expect = "RangeError: Error #1002";
    //captureThis();
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section J of test: no error intended!';
    actual = testNum5.toExponential(2);
    expect = '3.15e+5';
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
