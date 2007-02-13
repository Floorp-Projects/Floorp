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
 

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "const variables";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


import Package1.*;

use namespace ns1;

const item1:String = "const item1 set at creation time";

AddTestCase( "const variable item1", "const item1 set at creation time", item1);

AddTestCase( "package const variable packageItem1", "const packageItem1 set at creation time", packageItem1);
AddTestCase( "package const variable packageItem2", "const packageItem2 set at creation time", packageItem2);
AddTestCase( "package const variable packageItem3", undefined, packageItem3);
AddTestCase( "package const variable packageItem4", "const packageItem4 set at creation time", packageItem4);
AddTestCase( "package const variable packageItem5", 5, packageItem5);

var c1 = new Class1();

AddTestCase( "Class1 const variable classItem1", "const Class1 classItem1 set at creation time", c1.classItem1);
AddTestCase( "Class1 const variable classItem2", "const Class1 classItem2 set at creation time", c1.classItem2);
AddTestCase( "Class1 const variable classItem3", undefined, c1.classItem3);
AddTestCase( "Class1 const variable classItem4", "const Class1 classItem4 set at creation time", c1.classItem4);
AddTestCase( "Class1 const variable classItem5", 6, c1.classItem5);
AddTestCase( "Class1 const variable classItem6", "static const Class1 classItem6 set at creation time", Class1.classItem6);
AddTestCase( "Class1 const variable classItem7", "ns1 const Class1 classItem7 set at creation time", c1.classItem7);
AddTestCase( "Class1 const variable classItem8", "ns1 static const Class1 classItem8 set at creation time", Class1.classItem8);

var c2 = new Class2();

AddTestCase( "Class2 const variable classItem1", "const Class2 classItem1 set in constructor", c2.classItem1);
AddTestCase( "Class2 const variable classItem2", "const Class2 classItem2 set in constructor", c2.classItem2);
AddTestCase( "Class2 const variable classItem3", undefined, c2.classItem3);
AddTestCase( "Class2 const variable classItem4", "const Class2 classItem4 set in constructor", c2.classItem4);
AddTestCase( "Class2 const variable classItem5", 7, c2.classItem5);
AddTestCase( "Class2 const variable classItem6", "static const Class2 classItem6 set in function", Class2.classItem6);
AddTestCase( "Class2 const variable classItem7", "ns1 const Class2 classItem7 set in constructor", c2.classItem7);
AddTestCase( "Class2 const variable classItem8", "ns1 static const Class2 classItem8 set in function", Class2.classItem8);

test();       // leave this alone.  this executes the test cases and
              // displays results.
