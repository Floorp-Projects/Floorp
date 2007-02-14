/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   jrgm@netscape.com, pschwartau@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
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
