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


import UintPublicClassMethodArg.*;

var obj = new UintPublicClass();

AddTestCase( "Calling function with 1 uint argument", 1 , obj.oneArg(1) );
AddTestCase( "Calling function with 2 uint arguments", 3 , obj.twoArg(1,2) );
AddTestCase( "Calling function with 3 uint arguments", 6 , obj.threeArg(1,2,3) );
AddTestCase( "diffArg(uint, int, Number)", 6 , obj.diffArg(1,2,3) );
AddTestCase( "diffArg(int, uint, Number)", 6 , obj.diffArg2(1,2,3) );
AddTestCase( "diffArg(Number, int, uint)", 6 , obj.diffArg3(1,2,3) );
AddTestCase( "useProp(4)", 14 , obj.useProp(4) );
AddTestCase( "obj.pubProp = 10", 10 , obj.pubProp = 10 );
AddTestCase( "UintPublicClass.pubStatProp = 11", 11 , UintPublicClass.pubStatProp = 11 );

// precision runtime errors

var pResult = null;
try{
	obj.oneArg(-1);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "Precision runtime error one arg", "exception NOT caught", pResult );

pResult = null;
try{
	obj.twoArg(1,-2);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "Precision runtime error two args", "exception NOT caught", pResult );

pResult = null;
try{
	obj.threeArg(1,-2);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "Precision runtime error three args", "exception caught", pResult );

pResult = null;
try{
	obj.diffArg(-1,-2,-3);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "Precision runtime error diffArg", "exception NOT caught", pResult );

pResult = null;
try{
	obj.diffArg2(-1,-2,-3);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "Precision runtime error diffArg2", "exception NOT caught", pResult );

pResult = null;
try{
	obj.diffArg3(-1,-2,-3);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "Precision runtime error diffArg3", "exception NOT caught", pResult );

var n:Number = -20;

pResult = null;
try{
	obj.oneArg(n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; obj.oneArg(n);", "exception NOT caught", pResult );

pResult = null;
try{
	obj.twoArg(n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; obj.twoArg(n);", "exception caught", pResult );

pResult = null;
try{
	obj.twoArg(n,n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; obj.twoArg(n,n);", "exception NOT caught", pResult );

pResult = null;
try{
	obj.threeArg(n,n,n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; obj.threeArg(n,n,n);", "exception NOT caught", pResult );

pResult = null;
try{
	obj.diffArg(n,n,n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; obj.diffArg(n,n,n)", "exception NOT caught", pResult );

pResult = null;
try{
	obj.diffArg2(n,n,n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; obj.diffArg2(n,n,n)", "exception NOT caught", pResult );

pResult = null;
try{
	obj.diffArg3(n,n,n);
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "var n:Number = -20; obj.diffArg3(n,n,n)", "exception NOT caught", pResult );


// uint properties of the class

pResult = null;
try{
	obj.pubProp = -1;
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "obj.pubProp = -1", "exception NOT caught", pResult );

pResult = null;

try{
	UintPublicClass.pubStatProp = -1;
	pResult = "exception NOT caught";
} catch (e) {
	pResult = "exception caught";
}
AddTestCase( "UintPublicClass.pubStatProp = -1", "exception NOT caught", pResult );


test();       // leave this alone.  this executes the test cases and
              // displays results.

