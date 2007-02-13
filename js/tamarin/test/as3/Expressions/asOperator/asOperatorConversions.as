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

var SECTION = "Expressions";        // provide a document reference (ie, Actionscript section)
var VERSION = "AS3";        		// Version of ECMAScript or ActionScript
var TITLE   = "as operator";        // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

var string:String = "string" as String;
AddTestCase( "var string:String = 'string' as String", "string", string);

var number:Number = "string" as Number;
AddTestCase( "var number:Number = 'string' as Number", 0, number);

var myint:int = "string" as int;
AddTestCase( "var myint:int = 'string' as int", +0, myint);

var myuint:uint = "string" as uint;
AddTestCase( "var myuint:uint = 'string' as uint", +0, myuint);

var boolean:Boolean = "string" as Boolean;
AddTestCase( "var boolean:Boolean = 'string' as Boolean", false, boolean);

var object:Object = "string" as Object;
AddTestCase( "var object:Object = 'string' as Object", "string", object);

var string2:String = null as String;
AddTestCase( "var string:String = null as String", null, string2);

var number2:Number = null as Number;
AddTestCase( "var number:Number = null as Number", 0, number2);

var myint2:int = null as int;
AddTestCase( "var myint:int = null as int", +0, myint2);

var myuint2:uint = null as uint;
AddTestCase( "var myuint2:uint = null as uint", +0, myuint2);

var boolean2:Boolean = null as Boolean;
AddTestCase( "var boolean2:Boolean = null as Boolean", false, boolean2);

var object2:Object = null as Object;
AddTestCase( "var object2:Object = null as Object", null, object2);

var string3:String = undefined as String;
AddTestCase( "var string3:String = undefined as String", null, string3); // bug 131810

var number3:Number = undefined as Number;
AddTestCase( "var number3:Number = undefined as Number", 0, number3);

var myint3:int = undefined as int;
AddTestCase( "var myint3:int = undefined as int", +0, myint3);

var myuint3:uint = undefined as uint;
AddTestCase( "var myuint3:uint = undefined as uint", +0, myuint3);

var boolean3:Boolean = undefined as Boolean;
AddTestCase( "var boolean3:Boolean = undefined as Boolean", false, boolean3);

var object3:Object = undefined as Object;
AddTestCase( "var object3:Object = undefined as Object", null, object3);

var string4:String = true as String;
AddTestCase( "var string4:String = true as String", null, string4); // bug 131810

var number4:Number = true as Number;
AddTestCase( "var number4:Number = true as Number", 0, number4);

var myint4:int = true as int;
AddTestCase( "var myint4:int = true as int", 0, myint4);

var myuint4:uint = true as uint;
AddTestCase( "var myuint4:uint = true as uint", 0, myuint4);

var boolean4:Boolean = true as Boolean;
AddTestCase( "var boolean4:Boolean = true as Boolean", true, boolean4);

var object4:Object = true as Object;
AddTestCase( "var object4:Object = true as Object", true, object4);

var string5:String = false as String;
AddTestCase( "var string5:String = false as String", null, string5);

var number5:Number = false as Number;
AddTestCase( "var number5:Number = false as Number", 0, number5);

var myint5:int = false as int;
AddTestCase( "var myint5:int = false as int", +0, myint5);

var myuint5:uint = false as uint;
AddTestCase( "var myuint5:uint = false as uint", +0, myuint5);

var boolean5:Boolean = false as Boolean;
AddTestCase( "var boolean5:Boolean = false as Boolean", false, boolean5);

var object5:Object = false as Object;
AddTestCase( "var object5:Object = false as Object", false, object5);

var string6:String = 1.23 as String;
AddTestCase( "var string6:String = 1.23 as String", null, string6);

var number6:Number = 1.23 as Number;
AddTestCase( "var number6:Number = 1.23 as Number", 1.23, number6);

var myint6:int = 1.23 as int;
AddTestCase( "var myint6:int = 1.23 as int", 0, myint6);

var myuint6:uint = 1.23 as uint;
AddTestCase( "var myuint6:uint = 1.23 as uint", 0, myuint6);

var boolean6:Boolean = 1.23 as Boolean;
AddTestCase( "var boolean6:Boolean = 1.23 as Boolean", false, boolean6);

var object6:Object = 1.23 as Object;
AddTestCase( "var object6:Object = 1.23 as Object", 1.23, object6);

var string7:String = -1.23 as String;
AddTestCase( "var string7:String = -1.23 as String", null, string7);

var number7:Number = -1.23 as Number;
AddTestCase( "var number7:Number = -1.23 as Number", -1.23, number7);

var myint7:int = -1.23 as int;
AddTestCase( "var myint7:int = -1.23 as int", 0, myint7);

var myuint7:uint = -1.23 as uint;
AddTestCase( "var myuint7:uint = -1.23 as uint", 0, myuint7);

var boolean7:Boolean = -1.23 as Boolean;
AddTestCase( "var boolean7:Boolean = -1.23 as Boolean", false, boolean7);

var object7:Object = -1.23 as Object;
AddTestCase( "var object7:Object = -1.23 as Object", -1.23, object7);

var string8:String = NaN as String;
AddTestCase( "var string8:String = NaN as String", null, string8);

var number8:Number = NaN as Number;
AddTestCase( "var number8:Number = NaN as Number", NaN, number8);

var myint8:int = NaN as int;
AddTestCase( "var myint8:int = NaN as int", +0, myint8);

var myuint8:uint = NaN as uint;
AddTestCase( "var myuint8:uint = NaN as uint", +0, myuint8);

var boolean8:Boolean = NaN as Boolean;
AddTestCase( "var boolean8:Boolean = NaN as Boolean", false, boolean8);

var object8:Object = NaN as Object;
AddTestCase( "var object8:Object = NaN as Object", NaN, object8);


test();       // leave this alone.  this executes the test cases and
              // displays results.


