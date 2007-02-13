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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
*
*
* Date:    18 Feb 2002
* SUMMARY: Testing re.exec(str) when re.lastIndex is < 0 or > str.length
*
* Case 1: If re has the global flag set, then re(str) should be null
* Case 2: If re doesn't have this set, then re(str) should be unaffected
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=76717
*
*
* From the ECMA-262 Final spec:
*
* 15.10.6.2 RegExp.prototype.exec(string)
* Performs a regular expression match of string against the regular
* expression and returns an Array object containing the results of
* the match, or null if the string did not match.
*
* The string ToString(string) is searched for an occurrence of the
* regular expression pattern as follows:
*
* 1.  Let S be the value of ToString(string).
* 2.  Let length be the length of S.
* 3.  Let lastIndex be the value of the lastIndex property.
* 4.  Let i be the value of ToInteger(lastIndex).
* 5.  If the global property is false, let i = 0.
* 6.  If i < 0 or i > length then set lastIndex to 0 and return null.
* 7.  Call [[Match]], giving it the arguments S and i.
*     If [[Match]] returned failure, go to step 8;
*     otherwise let r be its State result and go to step 10.
* 8.  Let i = i+1.
* 9.  Go to step 6.
* 10. Let e be r's endIndex value.
* 11. If the global property is true, set lastIndex to e.
*
*          etc.
*
*
* So:
*
* A. If the global flag is not set, |lastIndex| is set to 0
*    before the match is attempted; thus the match is unaffected.
*
* B. If the global flag IS set and re.lastIndex is >= 0 and <= str.length,
*    |lastIndex| is incremented every time there is a match; not from
*    i to i+1, but from i to "endIndex" e:
*
*      e = (index of last input character matched so far by the pattern) + 1
*
*    The match is then attempted from this position in the string (Step 7).
*
* C. When the global flag IS set and re.lastIndex is < 0 or > str.length,
*    |lastIndex| is set to 0 and the match returns null.
*
*
* Note the |lastIndex| property is writeable, and may be set arbitrarily
* by the programmer - and we will do that below.
*
*/
//-----------------------------------------------------------------------------

var SECTION = "e15_10_6_2_2";
var VERSION = "";
var TITLE   = "Testing re.exec(str) when re.lastIndex is < 0 or > str.length";
var bug = "76717";

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


	/******************************************************************************
	 *
	 * Case 1 : when the global flag is set -
	 *
	 *****************************************************************************/
	pattern = /abc/gi;
	string = 'AbcaBcabC';

	status = inSection(1);
	actualmatch = pattern.exec(string);
	expectedmatch = Array('Abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(2);
	actualmatch = pattern.exec(string);
	expectedmatch = Array('aBc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(3);
	actualmatch = pattern.exec(string);
	expectedmatch = Array('abC');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	/*
	* At this point |lastIndex| is > string.length, so the match should be null -
	*/
	status = inSection(4);
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	/*
	* Now let's set |lastIndex| to -1, so the match should again be null -
	*/
	status = inSection(5);
	pattern.lastIndex = -1;
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	/*
	* Now try some edge-case values. Thanks to the work done in
	* http://bugzilla.mozilla.org/show_bug.cgi?id=124339, |lastIndex|
	* is now stored as a double instead of a uint32 (unsigned integer).
	*
	* Note 2^32 -1 is the upper bound for uint32's, but doubles can go
	* all the way up to Number.MAX_VALUE. So that's why we need cases
	* between those two numbers.
	*/
	/*
	***** leave commmented out until we decide the size of lastIndex ******

	status = inSection(6);
	pattern.lastIndex = Math.pow(2,32);
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(7);
	pattern.lastIndex = -Math.pow(2,32);
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(8);
	pattern.lastIndex = Math.pow(2,32) + 1;
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(9);
	pattern.lastIndex = -(Math.pow(2,32) + 1);
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(10);
	pattern.lastIndex = Math.pow(2,32) * 2;
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(11);
	pattern.lastIndex = -Math.pow(2,32) * 2;
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(12);
	pattern.lastIndex = Math.pow(2,40);
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(13);
	pattern.lastIndex = -Math.pow(2,40);
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(14);
	pattern.lastIndex = Number.MAX_VALUE;
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	status = inSection(15);
	pattern.lastIndex = -Number.MAX_VALUE;
	actualmatch = pattern.exec(string);
	expectedmatch = null;
	array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);
	*/


	/******************************************************************************
	 *
	 * Case 2: repeat all the above cases WITHOUT the global flag set.
	 * According to EMCA. |lastIndex| should get set to 0 before the match.
	 *
	 * Therefore re.exec(str) should be unaffected; thus our expected values
	 * below are now DIFFERENT when |lastIndex| is < 0 or > str.length
	 *
	 *****************************************************************************/

	pattern = /abc/i;
	string = 'AbcaBcabC';

	/*  status = inSection(16);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(17);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc'); // NOT Array('aBc') as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(18);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc'); // NOT Array('abC') as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/
	  /*
	   * At this point above, |lastIndex| WAS > string.length, but not here -
	   */
	/*  status = inSection(19);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/
	  /*
	   * Now let's set |lastIndex| to -1
	   */
	/*  status = inSection(20);
	  pattern.lastIndex = -1;
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/
	  /*
	   * Now try some edge-case values. Thanks to the work done in
	   * http://bugzilla.mozilla.org/show_bug.cgi?id=124339, |lastIndex|
	   * is now stored as a double instead of a uint32 (unsigned integer).
	   *
	   * Note 2^32 -1 is the upper bound for uint32's, but doubles can go
	   * all the way up to Number.MAX_VALUE. So that's why we need cases
	   * between those two numbers.
	   */
	/*  status = inSection(21);
	  pattern.lastIndex = Math.pow(2,32);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(22);
	  pattern.lastIndex = -Math.pow(2,32);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(23);
	  pattern.lastIndex = Math.pow(2,32) + 1;
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(24);
	  pattern.lastIndex = -(Math.pow(2,32) + 1);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(25);
	  pattern.lastIndex = Math.pow(2,32) * 2;
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(26);
	  pattern.lastIndex = -Math.pow(2,32) * 2;
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(27);
	  pattern.lastIndex = Math.pow(2,40);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -;
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(28);
	  pattern.lastIndex = -Math.pow(2,40);
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(29);
	  pattern.lastIndex = Number.MAX_VALUE;
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(30);
	  pattern.lastIndex = -Number.MAX_VALUE;
	  actualmatch = pattern.exec(string);
	  expectedmatch = Array('Abc') // NOT null as before -
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());
	*/

	// added from regress_24712
	status = inSection(31);
	pattern = /([\S]+([ \t]+[\S]+)*)[ \t]*=[ \t]*[\S]+/;
	actualmatch = pattern.exec("Course_Creator = Test");
	expectedmatch = ["Course_Creator = Test", "Course_Creator", undefined];
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
