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
* Date:    19 September 2002
* SUMMARY: Testing Array.prototype.concat()
* See http://bugzilla.mozilla.org/show_bug.cgi?id=169795
*
*/
//-----------------------------------------------------------------------------
	var SECTION = "15.4.4.4";
	var VERSION = "ECMA_1";
    var TITLE   = "Testing Array.prototype.concat()";
	var bug     = "169795";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);
    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

	var status = '';
	var actual = '';
	var expect= '';
	var x;

	status = inSection(1);
	x = "Hello";
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "concat(x).toString()", expect, actual );

	status = inSection(2);
	x = 999;
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "[].concat(999).toString()", expect, actual );

	status = inSection(3);
	x = /Hello/g;
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "[].concat(/Hello/g).toString()", expect, actual );

	status = inSection(4);
	x = new Error("Hello");
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "[].concat(new Error('hello')).toString()", expect, actual );


	/*
	status = inSection(5);
	x = function() {return "Hello";};
	actual = [].concat(x).toString();
	expect = x.toString();
	addThis();
	*/

	status = inSection(6);
	x = [function() {return "Hello";}];
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "[].concat([function() {return 'Hello';}).toString()", expect, actual );

	status = inSection(7);
	x = [1,2,3].concat([4,5,6]);
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "[].concat([1,2,3].concat([4,5,6]).toString()", expect, actual );

	status = inSection(8);
	x = this;
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "[].concat(this).toString()", expect, actual );

	/*
	 * The next two sections are by igor@icesoft.no; see
	 * http://bugzilla.mozilla.org/show_bug.cgi?id=169795#c3
	 */
	status = inSection(9);
	x={length:0};
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "[].concat({length:0}).toString()", expect, actual );

	status = inSection(10);
	x={length:2, 0:0, 1:1};
	actual = [].concat(x).toString();
	expect = x.toString();
	array[item++] = new TestCase(SECTION, "[].concat({length:2, 0:0, 1:1}).toString()", expect, actual );

    return ( array );
}
