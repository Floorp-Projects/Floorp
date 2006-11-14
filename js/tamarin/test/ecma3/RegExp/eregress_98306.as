/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either expressed
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
* Contributor(s): jrgm@netscape.com, pschwartau@netscape.com
* Date: 04 September 2001
*
* SUMMARY: Regression test for Bugzilla bug 98306
* "JS parser crashes in ParseAtom for script using Regexp()"
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=98306
*/
//-----------------------------------------------------------------------------

var SECTION = "eregress_98306";
var VERSION = "";
var TITLE   = "Testing that we don't crash on this code -";
var bug = "98306";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var cnUBOUND = 10;
	var re;
	var s;


	s = '"Hello".match(/[/]/)';
	tryThis(s);

	s = 're = /[/';
	tryThis(s);

	s = 're = /[/]/';
	tryThis(s);

	s = 're = /[//]/';
	tryThis(s);

	// Try to provoke a crash -
	function tryThis(sCode)
	{
	  var thisError = "no error";

	  // sometimes more than one attempt is necessary -
	  for (var i=0; i<cnUBOUND; i++)
	  {
	    try
	    {
	      sCode;
	    }
	    catch(e)
	    {
	      // do nothing; keep going -
	      thisError = "error";
	    }
	  }

	  array[item++] = new TestCase(SECTION, sCode, "no error", thisError);
	}

	return array;
}
