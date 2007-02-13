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

var SECTION = "Types: Conversions";
var VERSION = "as3";
var TITLE   = "implicit type conversions";

startTest();


// Value = NaN 

/*var thisError = "no exception thrown";
try{
	var string:String = NaN;
} catch (e0) {
	thisError = e0.toString();
} finally {
	AddTestCase( "var string:String = NaN", "no exception thrown", typeError(thisError));
	AddTestCase( "var string:String = NaN", "NaN", string);
}*/

var string:String = NaN;
AddTestCase( "var string:String = NaN", "NaN", string);

var number:Number = NaN;
AddTestCase("number:Number = NaN", NaN, number );

/*thisError = "no exception thrown";
try{
	var myInt:int = NaN;
} catch(e1) {
	thisError = e1.toString();
} finally {
	AddTestCase("myInt:int = NaN", "RangeError: Error #1061", rangeError(thisError) );
	AddTestCase("myInt:int = NaN", 0, myInt );
}*/

var myInt:int = NaN;
AddTestCase("myInt:int = NaN", 0, myInt );


/*thisError = "no exception thrown";
try{
	var myUint:uint = NaN;
} catch(e2) {
	thisError = e2.toString();
} finally {
	AddTestCase("myUInt:uint = NaN", "RangeError: Error #1061", rangeError(thisError) );
	AddTestCase("myUInt:uint = NaN", 0, myUint );	
}*/

var myUint:uint = NaN;
AddTestCase("myUInt:uint = NaN", 0, myUint );


/*thisError = "no exception thrown";
try{
	var boolean:Boolean = NaN;
} catch(e3) {
	thisError = e3.toString();
} finally {
	AddTestCase("boolean:Boolean = NaN", "no exception thrown", typeError(thisError) );
	AddTestCase("boolean:Boolean = NaN", false, boolean );
}*/

var boolean:Boolean = NaN;
AddTestCase("boolean:Boolean = NaN", false, boolean );

var object:Object = NaN;
AddTestCase( "var object:Object = NaN", NaN, object);


test();



