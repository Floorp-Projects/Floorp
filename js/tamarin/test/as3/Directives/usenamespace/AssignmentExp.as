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

var SECTION = "Directives";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS 3.0";        // Version of ECMAScript or ActionScript 
var TITLE   = "namespace assigned with AssignmentExpression";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone



class A{

	public namespace N1;
	public namespace N2;
	public namespace N3 = "foo";
	public namespace N4 = N3;

	var v1:int =1;
	var v2:int =2;
	var v3:int =5;
	N1 var v3;
	N2 var n2;
	N2 var n3
	N3 var v1 = 5;

	function a1() {
	     N1::v3=v1+v2;
	     return N1::v3;
	}
	function a2() {
	     N2::n2 = a1();
	     return N2::n2;
	}
	function a3() {
	     N2::n2 = v3;
	     return N2::n2;
	}
	function a4() {
	      return N4::v1;
	}
	function a5() {
		try {
			N1::v1=5;
			result = "no exception";
		} catch(e3) {
			result = e3.toString();
		}
		return result;
	}
	function a6() {
		N1::['v3']=4;
		return N1::['v3'];
	}
}

class C
{
	mx_internal var v:int = 0;
	mx_internal static var sv:int = 0;
}


var obj:A = new A();

 AddTestCase("N1::v3=3", 3, obj.a1());
 AddTestCase("N1::['v3']=4", 4, obj.a6());
 AddTestCase("N2::n2='3'", 3, obj.a2());
 AddTestCase("v3='5'", 5, obj.v3);
 AddTestCase("N2::n2='5'", 5, obj.a3());
 AddTestCase("N4 = N3; N4::v1", 5, obj.a4());
 
 namespace mx_internal;
 
use namespace mx_internal;
 
 var c:C = new C();
 
 try {
 	AddTestCase("c.v++", 1, (c.v++, c.v));
 	result = "no exception";
 } catch (e1) {
 	result = "exception";
 }
 AddTestCase("Increment in setter", "no exception", result);
 
 try {
 	AddTestCase("C.sv++", 1, (C.sv++, C.sv));
 	result = "no exception";
 } catch (e2) {
 	result = "exception";
 }
 AddTestCase("Increment in static setter", "no exception", result);
 
 AddTestCase("N1::v1=5 without declaring N2 var v1", "ReferenceError: Error #1056", referenceError(obj.a5()));

 test();
