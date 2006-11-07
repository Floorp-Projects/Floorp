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
* Software distributed under the License is distributed on an "AS  IS"
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
* Contributor(s): pschwartau@netscape.com
* Date: 17 September 2001
*
* SUMMARY: Regression test for Bugzilla bug 100199
* See http://bugzilla.mozilla.org/show_bug.cgi?id=100199
*
* The empty character class [] is a valid RegExp construct: the condition
* that a given character belong to a set containing no characters. As such,
* it can never be met and is always FALSE. Similarly, [^] is a condition
* that matches any given character and is always TRUE.
*
* Neither one of these conditions should cause syntax errors in a RegExp.
*/
//-----------------------------------------------------------------------------

var SECTION = "eregress_100199";
var VERSION = "";
var TITLE   = "[], [^] are valid RegExp conditions. Should not cause errors -";
var bug = "100199";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var status = '';
	var pattern = '';
	var string = '';
	var actualmatch = '';
	var expectedmatch = '';


	  /*pattern = /[]/;
	  string = 'abc';
	  status = inSection(1);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '';
	  status = inSection(2);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '[';
	  status = inSection(3);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '/';
	  status = inSection(4);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '[';
	  status = inSection(5);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = ']';
	  status = inSection(6);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '[]';
	  status = inSection(7);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '[ ]';
	  status = inSection(8);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '][';
	  status = inSection(9);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);


	pattern = /a[]/;
	  string = 'abc';
	  status = inSection(10);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '';
	  status = inSection(11);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = 'a[';
	  status = inSection(12);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = 'a[]';
	  status = inSection(13);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '[';
	  status = inSection(14);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = ']';
	  status = inSection(15);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '[]';
	  status = inSection(16);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '[ ]';
	  status = inSection(17);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '][';
	  status = inSection(18);
	  actualmatch = string.match(pattern);
	  expectedmatch = null;
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);


	pattern = /[^]/;
	  string = 'abc';
	  status = inSection(19);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('a');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = '';
	  status = inSection(20);
	  actualmatch = string.match(pattern);
	  expectedmatch = null; //there are no characters to test against the condition
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = '\/';
	  status = inSection(21);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('/');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = '\[';
	  status = inSection(22);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('[');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = '[';
	  status = inSection(23);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('[');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = ']';
	  status = inSection(24);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array(']');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = '[]';
	  status = inSection(25);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('[');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = '[ ]';
	  status = inSection(26);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('[');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = '][';
	  status = inSection(27);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array(']');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());*/


	pattern = /a[^]/;
	  string = 'abc';
	  status = inSection(28);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('ab');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = '';
	  status = inSection(29);
	  actualmatch = string.match(pattern);
	  expectedmatch = null; //there are no characters to test against the condition
	  array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	  string = 'a[';
	  status = inSection(30);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('a[');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = 'a]';
	  status = inSection(31);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('a]');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = 'a[]';
	  status = inSection(32);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('a[');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = 'a[ ]';
	  status = inSection(33);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('a[');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  string = 'a][';
	  status = inSection(34);
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('a]');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
