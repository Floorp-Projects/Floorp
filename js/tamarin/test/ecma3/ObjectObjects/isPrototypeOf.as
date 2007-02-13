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

	var SECTION = '15.2.4.6';
	var VERSION = 'no version';
    startTest();
	var TITLE = 'isPrototypeOf';

	writeHeaderToLog('Executing script: isPrototypeOf');
	writeHeaderToLog( SECTION + " "+ TITLE);

	var count = 0;
	var testcases = new Array();

	var re = new RegExp();


	testcases[count++] = new TestCase( SECTION, "RegExp.prototype.isPrototypeOf(re))", true,RegExp.prototype.isPrototypeOf(re));

	var str = new String("JScript");

	testcases[count++] = new TestCase( SECTION, "String.prototype.isPrototypeOf(str)", true,String.prototype.isPrototypeOf(str));

	testcases[count++] = new TestCase( SECTION, "String.prototype.isPrototypeOf(re)", false,String.prototype.isPrototypeOf(re));

    testcases[count++] = new TestCase( SECTION, "String.prototype.isPrototypeOf(new Number())", false,String.prototype.isPrototypeOf(new Number()));

	testcases[count++] = new TestCase( SECTION, "Object.prototype.isPrototypeOf(str)", true,Object.prototype.isPrototypeOf(str));

	testcases[count++] = new TestCase( SECTION, "Object.prototype.isPrototypeOf(re)", true,Object.prototype.isPrototypeOf(re));

    var myobj = new Object();


    testcases[count++] = new TestCase( SECTION, "String.prototype.isPrototypeOf(myobj)", false,String.prototype.isPrototypeOf(myobj));

    var myobj2 = null;

    testcases[count++] = new TestCase( SECTION, "Object.prototype.isPrototypeOf(myobj2)", false,Object.prototype.isPrototypeOf(myobj2));


	test();

