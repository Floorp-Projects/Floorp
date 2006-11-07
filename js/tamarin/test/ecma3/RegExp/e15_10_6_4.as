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
/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is JavaScript Engine testing utilities.
*
* The Initial Developer of the Original Code is Netscape Communications Corp.
* Portions created by the Initial Developer are Copyright (C) 2002
* the Initial Developer. All Rights Reserved.
*
* Contributor(s): rogerl@netscape.com, pschwartau@netscape.com
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK *****
*
*
* Date:    09 July 2002
* SUMMARY: RegExp conformance test
*
*   These testcases are derived from the examples in the ECMA-262 Ed.3 spec
*   scattered through section 15.10.2.
*
*/
//-----------------------------------------------------------------------------

var SECTION = "e15_10_2_1";
var VERSION = "";
var TITLE   = "RegExp conformance test";
var bug = "(none)";

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


	status = inSection(1);
	pattern = "a|ab";
	var regexp = new RegExp(pattern);
	string = 'abc';
	var regexp2 = pattern.toString();
	actualmatch = string.match(regexp2);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(2);
	pattern = "((a)|(ab))((c)|(bc))";
	string = 'abc';
	var regexp = new RegExp(pattern);
	regexp2 = pattern.toString();
	actualmatch = string.match(regexp2);
	expectedmatch = Array('abc', 'a', 'a', undefined, 'bc', undefined, 'bc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(3);
	pattern = "a[a-z]{2,4}";
	var regexp = new RegExp(pattern);
	string = 'abcdefghi';
	regexp2 = pattern.toString();
	actualmatch = string.match(regexp2);
	expectedmatch = Array('abcde');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(4);
	pattern = "a[a-z]{2,4}?";
	string = 'abcdefghi';
	var regexp = new RegExp(pattern);
	regexp2 = pattern.toString();
	actualmatch = string.match(regexp2);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(5);
	pattern = "(aa|aabaac|ba|b|c)*";
	var regexp = new RegExp(pattern);
	string = 'aabaac';
	regexp2 = pattern.toString();
	actualmatch = string.match(regexp2);
	expectedmatch = Array('aaba', 'ba');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());



	status = inSection(6);
	pattern = "(z)((a+)?(b+)?(c))*";
	string = 'zaacbbbcac';
	regexp = pattern.toString();
	actualmatch = string.match(regexp);
	expectedmatch = Array('zaacbbbcac', 'z', 'ac', 'a', undefined, 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(7);
	pattern = "(a*)*";
	string = 'b';
	regexp = pattern.toString();
	actualmatch = string.match(regexp);
	expectedmatch = Array('', "");
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());



	status = inSection(8);
	pattern = "(?=(a+))";
	string = 'baaabac';
	regexp = pattern.toString();
	actualmatch = string.match(regexp);
	expectedmatch = Array('', 'aaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());



	status = inSection(9);
	pattern = "(.*?)a(?!(a+)b\2c)\2(.*)";
	string = 'baaabaac';
	regexp = pattern.toString();
	actualmatch = string.match(regexp);

	/* This will need to be changed when bug 110184 is fixed
	//////////////////////////////
	*/

	//expectedmatch = Array('baaabaac', 'ba', undefined, 'abaac');
	expectedmatch = null;

	/*
	//////////////////////////////
	*/
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(10);
	pattern = "(?=(a+))";
	string = 'baaabac';
	var regexp = pattern.toString();
	actualmatch = string.match(regexp);
	expectedmatch = Array('', 'aaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
