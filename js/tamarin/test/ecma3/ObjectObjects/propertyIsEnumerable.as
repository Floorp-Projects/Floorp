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

	var SECTION = '15.2.4.7';
	var VERSION = 'no version';
	startTest();
	var TITLE = 'propertyIsEnumerable';

	writeHeaderToLog('Executing script: propertyIsEnumerable.as');
	writeHeaderToLog( SECTION + " "+ TITLE);

	var count = 0;
	var testcases = new Array();

	var a = new Array("apple", "banana", "cactus", "orange");


	testcases[count++] = new TestCase( SECTION, "a.propertyIsEnumerable(0)", true, a.propertyIsEnumerable(0));
	testcases[count++] = new TestCase( SECTION, "a.propertyIsEnumerable(1)", true, a.propertyIsEnumerable(1));
	testcases[count++] = new TestCase( SECTION, "a.propertyIsEnumerable(2)", true, a.propertyIsEnumerable(2));
	testcases[count++] = new TestCase( SECTION, "a.propertyIsEnumerable(3)", true, a.propertyIsEnumerable(3));
	testcases[count++] = new TestCase( SECTION, "a.propertyIsEnumerable(4)", false, a.propertyIsEnumerable(4));


	test();
