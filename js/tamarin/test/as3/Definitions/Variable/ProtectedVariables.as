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
var TITLE   = "protected variables";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


import Package1.*;

var c1 = new Class1();
c1.setClassItem1("Modified Class1 classItem1");
AddTestCase( "Class1 protected variable classItem1", "Modified Class1 classItem1", c1.getClassItem1());
AddTestCase( "Class1 protected const variable classItem2", "Class1 protected const classItem2 set at creation time", c1.getClassItem2());

Class1.setClassItem3("Modified Class1 classItem3");

AddTestCase( "Class1 protected static variable classItem3", "Modified Class1 classItem3", Class1.getClassItem3());
AddTestCase( "Class1 protected static const variable classItem4", "Class protected static const classItem4 set at creation time", Class1.getClassItem4());

var c2 = new Class2();

AddTestCase( "Class2 protected variable classItem1", "Class1 protected var classItem1 set at creation time", c2.getInheritedClassItem1());
AddTestCase( "Class2 protected const variable classItem2", "Class1 protected const classItem2 set at creation time", c2.getInheritedClassItem2());
AddTestCase( "Class2 protected static variable classItem3", "Modified Class1 classItem3", c2.getInheritedClassItem3());
AddTestCase( "Class2 protected static const variable classItem4", "Class protected static const classItem4 set at creation time", c2.getInheritedClassItem4());

test();       // leave this alone.  this executes the test cases and
              // displays results.
