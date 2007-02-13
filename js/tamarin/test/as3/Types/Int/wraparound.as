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

var SECTION = "Wraparound_Conversion";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS3";        // Version of ECMAScript or ActionScript 
var TITLE   = "Wraparound_Conversion";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


///////////////////////////////////////////////////////////////
// add your tests here

/* 
	Reference max / min vals:
	int.MAX_VALUE: 2147483647
	int.MIN_VALUE: -2147483648
	uint.MAX_VALUE: 4294967295
	uint.MIN_VALUE: 0
	Number.MAX_VALUE: 1.79769313486231e+308
	Number.MIN_VALUE: 4.9406564584124654e-324
*/

//simple wraparound tests

//int
var intNum:int;

var intMaxWrapAdd:int = int.MAX_VALUE + 1;
AddTestCase( "int.MAX_VALUE + 1 = -2147483648", -2147483648, intMaxWrapAdd );

intNum = int.MAX_VALUE;
intNum = intNum + 1;
AddTestCase( "int.MAX_VALUE + 1 = -2147483648", -2147483648, intNum );

intNum = int.MAX_VALUE;
intNum++;
AddTestCase( "increment int at int.MAX_VALUE", -2147483648, intNum );

var intMinWrapAdd:int = int.MIN_VALUE - 1;
AddTestCase( "int.MIN_VALUE - 1 = 2147483647", 2147483647, intMinWrapAdd );

intNum = int.MIN_VALUE;
intNum = intNum - 1;
AddTestCase( "int.MIN_VALUE - 1 = 2147483647", 2147483647, intNum );

intNum = int.MIN_VALUE;
intNum--;
AddTestCase( "decrement int at int.MIN_VALUE", 2147483647, intNum );

var intMaxWrapMult:int = int.MAX_VALUE * 2;
AddTestCase( "int.MAX_VALUE * 2 = -2", -2, intMaxWrapMult );

//uint
var uintNum:uint;

var uintMaxWrapAdd:uint = uint.MAX_VALUE + 1;
AddTestCase( "uint.MAX_VALUE + 1 = 0", 0, uintMaxWrapAdd );

uintNum = uint.MAX_VALUE;
uintNum = uintNum + 1;
AddTestCase( "uint.MAX_VALUE + 1 = 0", 0, uintNum );

uintNum = uint.MAX_VALUE;
uintNum++;
AddTestCase( "increment uint at uint.MAX_VALUE", 0, uintNum );

var uintMinWrapAdd:uint = uint.MIN_VALUE - 1;
AddTestCase( "uint.MIN_VALUE - 1 = 4294967295", 4294967295, uintMinWrapAdd );

uintNum = uint.MIN_VALUE;
uintNum = uintNum - 1;
AddTestCase( "uint.MIN_VALUE - 1 = 4294967295", 4294967295, uintNum );

uintNum = uint.MIN_VALUE;
uintNum--;
AddTestCase( "decrement uint at uint.MIN_VALUE", 4294967295, uintNum );

var uintMaxWrapMult:uint = uint.MAX_VALUE * 2;
AddTestCase( "uint.MAX_VALUE * 2 = 4294967294", 4294967294, uintMaxWrapMult );

//bitwise shift tests

intNum = int.MAX_VALUE;

intNum = intNum << 1;
AddTestCase( "int.MAX_VALUE << 1", -2, intNum );

uintNum = uint.MAX_VALUE;

// uint.MAX_VALUE << 1 (FFFFFFFF << 1 = 1FFFFFFFE) then to wraparound: 1FFFFFFFE % 100000000 which gives FFFFFFFE.
uintNum = uintNum << 1;
AddTestCase( "uint.MAX_VALUE << 1", 4294967294, uintNum );


//
////////////////////////////////////////////////////////////////

test();       // leave this alone.  this executes the test cases and
              // displays results.
			  
			  
			  
			  
			  
			  
