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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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
 //-----------------------------------------------------------------------------
var SECTION = "RegExp/properties-002.js";
var VERSION = "ECMA_2";
var TITLE   = "Properties of RegExp Instances";
var BUGNUMBER ="http://scopus/bugsplat/show_bug.cgi?id=346032";
// ALSO SEE http://bugzilla.mozilla.org/show_bug.cgi?id=124339

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	re_1 = /\cA?/g;
	re_1.lastIndex = Math.pow(2,30);
	AddRegExpCases( re_1, "\\cA?", true, false, false, Math.pow(2,30) );

	re_2 = /\w*/i;
	re_2.lastIndex = Math.pow(2,30) -1;
	AddRegExpCases( re_2, "\\w*", false, true, false, Math.pow(2,30)-1 );

	re_3 = /\*{0,80}/m;
	re_3.lastIndex = Math.pow(2,30) -1;
	AddRegExpCases( re_3, "\\*{0,80}", false, false, true, Math.pow(2,30) -1 );

	re_4 = /^./gim;
	re_4.lastIndex = Math.pow(2,30) -1;
	AddRegExpCases( re_4, "^.", true, true, true, Math.pow(2,30) -1 );

	re_5 = /\B/;
	re_5.lastIndex = Math.pow(2,30);
	AddRegExpCases( re_5, "\\B", false, false, false, Math.pow(2,30) );

	/*
	 * Brendan: "need to test cases Math.pow(2,32) and greater to see
	 * whether they round-trip." Reason: thanks to the work done in
	 * http://bugzilla.mozilla.org/show_bug.cgi?id=124339, lastIndex
	 * is now stored as a double instead of a uint32 (unsigned integer).
	 *
	 * Note 2^32 -1 is the upper bound for uint32's, but doubles can go
	 * all the way up to Number.MAX_VALUE. So that's why we need cases
	 * between those two numbers.
	 *
	 */
	re_6 = /\B/;
	re_6.lastIndex = Math.pow(2,30);
	AddRegExpCases( re_6, "\\B", false, false, false, Math.pow(2,30) );

	re_7 = /\B/;
	re_7.lastIndex = Math.pow(2,29) + 1;
	AddRegExpCases( re_7, "\\B", false, false, false, Math.pow(2,29) + 1 );

	re_8 = /\B/;
	re_8.lastIndex = Math.pow(2,29) * 2;
	AddRegExpCases( re_8, "\\B", false, false, false, Math.pow(2,29) * 2 );

	re_9 = /\B/;
	re_9.lastIndex = Math.pow(2,30);
	AddRegExpCases( re_9, "\\B", false, false, false, Math.pow(2,30) );


	function AddRegExpCases( re, s, g, i, m, l ){

	    array[item++] = new TestCase(SECTION,
	    			 re + ".test == RegExp.prototype.test",
	                 true,
	                 re.test == RegExp.prototype.test );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".toString == RegExp.prototype.toString",
	                 true,
	                 re.toString == RegExp.prototype.toString );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".constructor == RegExp.prototype.constructor",
	                 true,
	                 re.constructor == RegExp.prototype.constructor );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".compile == RegExp.prototype.compile",
	                 true,
	                 re.compile == RegExp.prototype.compile );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".exec == RegExp.prototype.exec",
	                 true,
	                 re.exec == RegExp.prototype.exec );

	    // properties

	    array[item++] = new TestCase(SECTION,
	    			 re + ".source",
	                 s,
	                 re.source );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".toString()",
	                 "/" + s +"/" + (g?"g":"") + (i?"i":"") +(m?"m":""),
	                 re.toString() );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".global",
	                 g,
	                 re.global );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".ignoreCase",
	                 i,
	                 re.ignoreCase );

	    array[item++] = new TestCase(SECTION,
	    			 re + ".multiline",
	                 m,
	                 re.multiline);

	    array[item++] = new TestCase(SECTION,
	    			 re + ".lastIndex",
	                 l,
	                 re.lastIndex  );
	}

	return array;
}
