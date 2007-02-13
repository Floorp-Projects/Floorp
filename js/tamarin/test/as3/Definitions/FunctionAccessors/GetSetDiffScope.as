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

 import GetSetDiffScope.*;


 var SECTION = "FunctionAccessors";
 var VERSION = "AS3";
 var TITLE   = "Function Accessors";
 var BUGNUMBER = "143360";


startTest();


w = new GetSetDiffScope();


try {
	w.g1 = {a:1, b:2, c:3};
	errorNumber = -1;
	errorType = undefined;
} catch(e1) {
	errorNumber = referenceError(e1.toString());
	errorType = e1 is ReferenceError;

}


AddTestCase("Setter is missing","ReferenceError: Error #1074", errorNumber);
AddTestCase("Make sure property value hasn't changed", "original value", w.g1);


try {
	w.g2 = {a:1, b:2, c:3};
	errorNumber = -1;
	errorType = undefined;
} catch(e2) {
	errorNumber = referenceError(e2.toString());
	errorType = e2 is ReferenceError;

}

AddTestCase("Setter is private", "ReferenceError: Error #1074", errorNumber);
AddTestCase("Make sure property value hasn't changed", 5, w.g2);


try {
	somevar = w.s1;
	errorNumber = -1;
	errorType = undefined;
} catch(e3) {
	errorNumber = referenceError(e3.toString());
	errorType = e3 is ReferenceError;

}

AddTestCase("Setter is missing, error 1069", "ReferenceError: Error #1069", errorNumber);
AddTestCase("Checking for correct error type ReferenceError", true, errorType);

try {
	somevar = w.s2;
	errorNumber = -1;
	errorType = undefined;
} catch(e4) {
	errorNumber = referenceError(e4.toString());
	errorType = e4 is ReferenceError;

}

AddTestCase("Setter is private, error 1077", "ReferenceError: Error #1077", errorNumber);
AddTestCase("Checking for correct error type ReferenceError", true, errorType);

var t:testclass = new testclass();

try {
	somevar = t.doGet();
	errorNumber = -1;
	errorType = undefined;
} catch (e1) {
	errorNumber = referenceError(e1.toString());
	errorType = e1 is ReferenceError;
}
AddTestCase("Public getter, namespace setter; get", 1, somevar);
AddTestCase("Checking error did not occur, error number", -1, errorNumber);
AddTestCase("Checking error did not occur, error type", undefined, errorType);

try {
	somevar = t.doSet();
	errorNumber = -1;
	errorType = undefined;
} catch (e2) {
	errorNumber = referenceError(e2.toString());
	errorType = e2 is ReferenceError;
}

AddTestCase("Public getter, namespace setter, set; error 1074", "ReferenceError: Error #1074", errorNumber);
AddTestCase("Checking for correct error type ReferenceError", true, errorType);


test();
