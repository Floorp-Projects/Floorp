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
* Contributor(s): jband@netscape.com, pschwartau@netscape.com
* Date: 14 October 2001
*
* SUMMARY: Regression test for Bugzilla bug 104584
* See http://bugzilla.mozilla.org/show_bug.cgi?id=104584
*
* Testing that we don't crash on this code. The idea is to
* call F,G WITHOUT providing an argument. This caused a crash
* on the second call to obj.toString() or print(obj) below -
*/
//-----------------------------------------------------------------------------
var SECTION = "regress_104584";       // provide a document reference (ie, ECMA section)
var VERSION = "";  // Version of JavaScript or ECMA
var TITLE   = "Testing that we don't crash on this code -";       // Provide ECMA section title or a description
var BUGNUMBER = "104584";

startTest();

var testcases = getTestCases()
test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    F();
    G();
    
    function F(obj)
    {
      if(!obj)
        obj = {};
      obj.toString();
      //gc();
      obj.toString();
    }
    
    
    function G(obj)
    {
      if(!obj)
        obj = {};
      trace(obj);
      //gc();
      trace(obj);
    }
    
    array[item++] = new TestCase(SECTION, "Make sure we don't crash", true, true);
    
    return array;
}
