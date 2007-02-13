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
 
 

import testdynfinalClassWithMultiStringParamCons.*;

var SECTION = "Definitions";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Constructors with parameters of a Dynamic class";  // Provide ECMA section                                                                   //title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


var x = "test";
var y = "hello"+"world"; 
var dynWithStrParamCons= new dynfinClassWithMultiStrParamCons(x);
var g2=new dynfinClassWithMultiStrParamCons(x+y);
//print(dynWithStrParamCons.myString());
//print(dynWithStrParamCons.myString2());
//print(g2.myString());
AddTestCase("calling public Instance method for testing constructor with single string parameter","test",dynWithStrParamCons.myString());
AddTestCase("calling public Instance method for testing constructor with concatenated string","helloworld",dynWithStrParamCons.myString2());
AddTestCase("calling public Instance method for testing constructor with concatenated string","testhelloworld",g2.myString());


test();       // leave this alone.  this executes the test cases and
              // displays results.
