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

	var SECTION = '15.2.4.5';
	var VERSION = 'no version';
	startTest();
	var TITLE = 'hasOwnProperty';

	writeHeaderToLog('Executing script: hasOwnProperty.as');
	writeHeaderToLog( SECTION + " "+ TITLE);

	var count = 0;
	var testcases = new Array();


	testcases[count++] = new TestCase( SECTION, "String.prototype.hasOwnProperty(\"split\")", true,String.prototype.hasOwnProperty("split"));

	var str = new String("JScript");

	testcases[count++] = new TestCase( SECTION, "str.hasOwnProperty(\"split\")", false,str.hasOwnProperty("split"));

	testcases[count++] = new TestCase( SECTION, "Array.prototype.hasOwnProperty(\"pop\")", true,Array.prototype.hasOwnProperty("pop"));

	testcases[count++] = new TestCase( SECTION, "Number.prototype.hasOwnProperty(\"toPrecision\")", true,Number.prototype.hasOwnProperty("toPrecision"));

	testcases[count++] = new TestCase( SECTION, "Date.prototype.hasOwnProperty(\"getTime\")", true,Date.prototype.hasOwnProperty("getTime"));

	testcases[count++] = new TestCase( SECTION, "RegExp.prototype.hasOwnProperty(\"exec\")", true,RegExp.prototype.hasOwnProperty("exec"));

	testcases[count++] = new TestCase( SECTION, "String.prototype.hasOwnProperty(\"random\")", false,String.prototype.hasOwnProperty("random"));

	testcases[count++] = new TestCase( SECTION, "Object.prototype.hasOwnProperty(\"constructor\")", true,Object.prototype.hasOwnProperty("constructor"));

    testcases[count++] = new TestCase( SECTION, "Object.prototype.hasOwnProperty(\"getTime\")", false,Object.prototype.hasOwnProperty("getTime"));

    var myobj = new Object();

    testcases[count++] = new TestCase( SECTION, "myobj.hasOwnProperty(\"constructor\")", false,myobj.hasOwnProperty("constructor"));


	test();
