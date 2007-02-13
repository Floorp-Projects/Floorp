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
 
 var SECTION = "FunctionAccessors";
 var VERSION = "AS3"; 
 var TITLE   = "Function Accessors";
 var BUGNUMBER = "106381";
 
startTest();

class A
{
var _v:Number = 0;

function get v():Number
{
return _v;
}

function set v(value:Number)
{
_v = value;
}
}

class B extends A
{
override function get v():Number
{

return super.v + 1; // this caused infinite recursion per bug 106381
}
}

var b:B = new B();
try{
	var res = "not run";
	b.v; // should cause infinite recursion
	res = "no exception";
} catch (e) {
	res = "exception";
} finally {
	AddTestCase("Getter calling super", "no exception", res);
}

try{
	var res = "not run";
	b.v = 1;
	res = "no exception";
	AddTestCase("Setting value whose getter calls super", 2, b.v);
} catch (e) {
	res = "exception";
} finally {
	AddTestCase("Infinite recursion getter calling super", "no exception", res);
}

test();
