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

startTest();                // leave this alone


var num1:uint;
var num2:uint;
var num3:uint;
var num4:Number;
var num5:Number;

num1 = 20;
num2 = 5;
num4 = 10;
num5 = -100;

AddTestCase( "Uint addition", 25, (num3 = (num1 + num2)) );
AddTestCase( "Uint subtraction", 15, (num3 = (num1 - num2)) );
AddTestCase( "Uint multiplication", 100, (num3 = (num1 * num2)) );
AddTestCase( "Uint division", 4, (num3 = (num1 / num2)) );

AddTestCase( "Adding uint and Number variables", 30, (num3 = (num1 + num4)) );
AddTestCase( "Subtracting Number variable from uint variable", 10, (num3 = (num1 - num4)) );
AddTestCase( "Multiplying uint and Number variable", 200, (num3 = (num1 * num4)) );
AddTestCase( "Diving uint by a Number variable", 2, (num3 = (num1 / num4)) );

AddTestCase( "Adding uint and Number variables", 21, (num3 = (num1 + 1)) );
AddTestCase( "Subtracting Number variable from uint variable", 0, (num3 = (num1 - 20)) );
AddTestCase( "Multiplying uint and Number variable", 140, (num3 = (num1 * 7)) );
AddTestCase( "Diving uint by a Number variable", 2, (num3 = (num1 / 10)) );

// RangeError precision test cases
var pResult = null;
try{
	var temp:uint;
	temp = num1 + num5;
	pResult = "exception NOT caught";
} catch(e) {
	pResult = "exception caught";
}
AddTestCase( "var temp:uint; num1(+20) + num5(-100)", "exception NOT caught", pResult );

pResult = null;

try{
	var temp:uint;
	temp = -100;
	pResult = "exception NOT caught";
} catch(e) {
	pResult = "exception caught";
}
AddTestCase( "var temp:uint; temp = -100", "exception NOT caught", pResult );

//divide
pResult = null;
try{
	var temp:uint;
	temp = -100/2;
	pResult = "exception NOT caught";
} catch(e) {
	pResult = "exception caught";
}
AddTestCase( "var temp:uint; temp = -100/2", "exception NOT caught", pResult );

// multiply
pResult = null;
try{
	var temp:uint;
	temp = -100*2;
	pResult = "exception NOT caught";
} catch(e) {
	pResult = "exception caught";
}
AddTestCase( "var temp:uint; temp = -100*2", "exception NOT caught", pResult );

// subtract
pResult = null;
try{
	var temp:uint;
	temp = 1-100;
	pResult = "exception NOT caught";
} catch(e) {
	pResult = "exception caught";
}
AddTestCase( "var temp:uint; temp = 1-100", "exception NOT caught", pResult );


test();       // leave this alone.  this executes the test cases and
              // displays results.
