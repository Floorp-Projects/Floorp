/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
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
* Date: 13 August 2001
*
* SUMMARY: Invoking an undefined function should produce a ReferenceError
* See http://bugzilla.mozilla.org/show_bug.cgi?id=95101
*/
//-----------------------------------------------------------------------------
    var SECTION = "regress_95101";
    var VERSION = "";
    var bug = 95101;
    var TITLE   = "Invoking an undefined function should produce a ReferenceError";
    var msgERR_REF_YES = 'ReferenceError';
    var msgERR_REF_NO = 'did NOT generate a ReferenceError';

    printBugNumber (bug);
    printStatus (TITLE);

    startTest();
    writeHeaderToLog( SECTION + " Number.prototype.valueOf()");
    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;  
    var UBound = 0;
    var status = '';
    var statusitems = [];
    var actual = '';
    var actualvalues = [];
    var expect= '';
    var expectedvalues = [];


    try
    {
      xxxyyyzzz();
    }
    catch (e)
    {
      status = 'Section 1 of test';
      actual = e instanceof ReferenceError;
      expect = true;
      array[item++] = new TestCase(SECTION, status, expect, actual);
    
    
      /*
       * This test is more literal, and may one day be invalid.
       * Searching for literal string "ReferenceError" in e.toString()
       */
      status = 'Section 2 of test';
      var match = e.toString().search(/ReferenceError/);
      actual = (match > -1);
      expect = true;
      array[item++] = new TestCase(SECTION, status, expect, actual);
    }
    return array;
}


// converts a Boolean result into a textual result -
function isReferenceError(bResult)
{
  return bResult? msgERR_REF_YES : msgERR_REF_NO;
}
