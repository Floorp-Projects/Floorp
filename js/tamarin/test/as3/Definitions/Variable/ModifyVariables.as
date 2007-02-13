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
var TITLE   = "Modify variable after they are created";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone




import Package1.*;

use namespace ns1;

var item1:String = "item1 set at creation time";
var item2 = "item2 set at creation time", item3, item4 = "item4 set at creation time";
var item5:int = 4;

item1 = "item1 modified";
item2 = "item2 modified";
item3 = "item3 modified";
item4 = "item4 modified";
item5 = 3;

AddTestCase( "Modify global variable item1", "item1 modified", item1);
AddTestCase( "Modify global variable item2", "item2 modified", item2);
AddTestCase( "Modify global variable item3", "item3 modified", item3);
AddTestCase( "Modify global variable item4", "item4 modified", item4);
AddTestCase( "Modify global variable item5", 3, item5);

packageItem1 = "packageItem1 modified";
packageItem2 = "packageItem2 modified";
packageItem3 = "packageItem3 modified";
packageItem4 = "packageItem4 modified";
packageItem5 = 2;

AddTestCase( "Modify package variable packageItem1", "packageItem1 modified", packageItem1);
AddTestCase( "Modify package variable packageItem2", "packageItem2 modified", packageItem2);
AddTestCase( "Modify package variable packageItem3", "packageItem3 modified", packageItem3);
AddTestCase( "Modify package variable packageItem4", "packageItem4 modified", packageItem4);
AddTestCase( "Modify package variable packageItem5", 2, packageItem5);

var c1 = new Class1();
c1.classItem1 = "Class1 classItem1 modified";
c1.classItem2 = "Class1 classItem2 modified";
c1.classItem3 = "Class1 classItem3 modified";
c1.classItem4 = "Class1 classItem4 modified";
c1.classItem5 = 1;
Class1.classItem6 = "Class1 static classItem6 modified";
c1.classItem7 = "ns1 Class1 classItem7 modified";

AddTestCase( "Modify Class1 variable classItem1", "Class1 classItem1 modified", c1.classItem1);
AddTestCase( "Modify Class1 variable classItem2", "Class1 classItem2 modified", c1.classItem2);
AddTestCase( "Modify Class1 variable classItem3", "Class1 classItem3 modified", c1.classItem3);
AddTestCase( "Modify Class1 variable classItem4", "Class1 classItem4 modified", c1.classItem4);
AddTestCase( "Modify Class1 variable classItem5", 1, c1.classItem5);
AddTestCase( "Modify Class1 variable classItem6", "Class1 static classItem6 modified", Class1.classItem6);
AddTestCase( "Modify Class1 variable classItem7", "ns1 Class1 classItem7 modified", c1.classItem7);

var c2 = new Class2();
c2.classItem1 = "Class2 classItem1 modified";
c2.classItem2 = "Class2 classItem2 modified";
c2.classItem3 = "Class2 classItem3 modified";
c2.classItem4 = "Class2 classItem4 modified";
c2.classItem5 = -1;
Class2.classItem6 = "Class2 static classItem6 modified";
c2.classItem7 = "ns1 Class2 classItem7 modified";
Class2.classItem8 = "ns1 Class2 static classItem8 modified";

AddTestCase( "Modify Class2 variable classItem1", "Class2 classItem1 modified", c2.classItem1);
AddTestCase( "Modify Class2 variable classItem2", "Class2 classItem2 modified", c2.classItem2);
AddTestCase( "Modify Class2 variable classItem3", "Class2 classItem3 modified", c2.classItem3);
AddTestCase( "Modify Class2 variable classItem4", "Class2 classItem4 modified", c2.classItem4);
AddTestCase( "Modify Class2 variable classItem5", -1, c2.classItem5);
AddTestCase( "Modify Class2 variable classItem6", "Class2 static classItem6 modified", Class2.classItem6);
AddTestCase( "Modify Class2 variable classItem7", "ns1 Class2 classItem7 modified", c2.classItem7);
AddTestCase( "Modify Class2 variable classItem8", "ns1 Class2 static classItem8 modified", Class2.classItem8);

test();       // leave this alone.  this executes the test cases and
              // displays results.
