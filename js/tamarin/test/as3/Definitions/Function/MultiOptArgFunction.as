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

import MultiOptArgFunction.*

class MultiOptArgFunctionClass {
 	function returnArguments(s:String = "Str3", b:Boolean = true, n:Number = 30) {
 		str = s;
 		bool = b;
 	  	num = n;
 	}
}

function returnArgumentsNoPackage(s:String = "Str4", b:Boolean = false, n:Number = 40) {
	str = s;
	bool = b;
	num = n;
}


var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "Function Body Parameter/Result Type";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


var TESTOBJ = new TestObj();
var TESTOBJ1 = new MultiOptArgFunctionClass();

var success = false;
TESTOBJ.returnArguments("String1");

if(str == "String1" && bool == true && num == 10)
{ success = true;}
else
{ success = false;}

AddTestCase( "TESTOBJ.returnArguments('String1');", true, success );


success = false;
TESTOBJ.returnArguments("String1",false);

if(str == "String1" && bool == false && num == 10)
{ success = true;}
else
{ success = false;}

AddTestCase( "TESTOBJ.returnArguments('String1',false)", true, success );


success = false;
TESTOBJ.returnArguments("String1",false,100);

if(str == "String1" && bool == false && num == 100)
{ success = true;}
else
{success = false;}

AddTestCase( "TESTOBJ.returnArguments('String1',false,100);", true, success );


success = false;
returnArguments("String2");

if(str == "String2" && bool == false && num == 20)
{success = true;}
else
{success = false;}

AddTestCase( "returnArguments('String2')", true, success );


success = false;
returnArguments("String2",true);

if(str == "String2" && bool == true && num == 20)
{success = true;}
else
{success = false;}

AddTestCase( "returnArguments('String2',true)", true, success );

success = false;
returnArguments("String2",true,100);

if(str == "String2" && bool == true && num == 100)
{success = true;}
else
{success = false;}

AddTestCase( "returnArguments('String2',true,100)", true, success );

success = false;
TESTOBJ1.returnArguments("String3");

if(str == "String3" && bool == true && num == 30)
{success = true;}
else
{success = false;}

AddTestCase( "TESTOBJ1.returnArguments('String3')", true, success );


success = false;
TESTOBJ1.returnArguments("String3",false);

if(str == "String3" && bool == false && num == 30)
{success = true;}
else
{success = false;}

AddTestCase( "TESTOBJ1.returnArguments('String3',false)", true, success );

success = false;
TESTOBJ1.returnArguments("String3",false,300);

if(str == "String3" && bool == false && num == 300)
{success = true;}
else
{success = false;}

AddTestCase( "TESTOBJ1.returnArguments('String3',false,300)", true, success );


success = false;
returnArgumentsNoPackage("String4");

if(str == "String4" && bool == false && num == 40)
{success = true;}
else
{success = false;}

AddTestCase( "returnArgumentsNoPackage('String4')", true, success );


success = false;
returnArgumentsNoPackage("String4",true);

if(str == "String4" && bool == true && num == 40)
{success = true;}
else
{success = false;}

AddTestCase( "returnArgumentsNoPackage('String4',true)", true, success );


success = false;
returnArgumentsNoPackage("String4",true,400);

if(str == "String4" && bool == true && num == 400)
{success = true;}
else
{success = false;}

AddTestCase( "returnArgumentsNoPackage('String4',true,400)", true, success );


test();       // leave this alone.  this executes the test cases and
              // displays results.
