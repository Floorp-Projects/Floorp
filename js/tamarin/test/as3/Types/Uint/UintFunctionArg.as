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



function Uint1Arg(n1:uint):uint {
 
 var n2:uint = n1;
 
 return n2;

}

function UintMultiArgs(n1:uint, n2:uint, n3:uint):uint {
 
 var n4:uint = n1 + n2 + n3;
 
 return n4;

}

// mt: adding different types of args test cases
function diffArgs( arg1:int, arg2:uint, arg3:Number ):uint{
	return arg1+arg2+arg3;
} 

function returnNegUint():uint {
	return -10;
}

function addNegUintInFunc(){
	var a:uint;
	var b = -100;
	var c = 1;
	return (a = b+c);
}

AddTestCase( "Calling function with 1 uint argument", 1 , Uint1Arg(1) );
AddTestCase( "Calling function with 1 uint argument", 6 , UintMultiArgs(1,2,3) );

// RangeError precision exceptions

var pResult = null;
try{
	Uint1Arg(-1);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "Uint1Arg(-1)", "exception NOT caught" , pResult );
AddTestCase( "Uint1Arg(-1)", 4294967295 , Uint1Arg(-1) );
pResult = null;
try{
	UintMultiArgs(-1,-1,-1);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "UintMultiArgs(-1,-1,-1)", "exception NOT caught" , pResult );
AddTestCase( "UintMultiArgs(-1,-1,-1)", 4294967293, UintMultiArgs(-1,-1,-1) )
pResult = null;
try{
	diffArgs(-1,-1,-1);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "diffArgs(-1,-1,-1)", "exception NOT caught" , pResult );
AddTestCase( "diffArgs(-1,-1,-1)", 4294967293 , diffArgs(-1,-1,-1) );
pResult = null;
try{
	returnNegUint();
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "returnNegUint()", "exception NOT caught" , pResult );
AddTestCase( "returnNegUint()", 4294967286 , returnNegUint() );

var n:Number = -20;

pResult = null;
try{
	Uint1Arg(n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; Uint1Arg(n)", "exception NOT caught" , pResult );



pResult = null;
try{
	UintMultiArgs(n,n,n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; UintMultiArgs(n,n,n)", "exception NOT caught" , pResult );

pResult = null;
try{
	diffArgs(n,n,n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; diffArgs(n,n,n)", "exception NOT caught" , pResult );

var i:int = -20;

pResult = null;
try{
	Uint1Arg(i);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var i:int = -20; Uint1Arg(i)", "exception NOT caught" , pResult );

pResult = null;
try{
	UintMultiArgs(i,i,i);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var i:int = -20; UintMultiArgs(i,i,i)", "exception NOT caught" , pResult );

pResult = null;
try{
	diffArgs(i,i,i);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var i:int = -20; diffArgs(i,i,i)", "exception NOT caught" , pResult );

pResult = null;
try{
	addNegUintInFunc();
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "add negitive number to uint in function", "exception NOT caught" , pResult );

test();       // leave this alone.  this executes the test cases and
              // displays results.
