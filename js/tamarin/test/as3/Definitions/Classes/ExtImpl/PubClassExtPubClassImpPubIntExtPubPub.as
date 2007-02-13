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
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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

import PubClassExtPubClassImpPubIntExtPubPub.*;
var SECTION = "Definitions";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS3";        // Version of ECMAScript or ActionScript 
var TITLE   = "Public class implements public interface";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

/**
 * Calls to AddTestCase here. AddTestCase is a function that is defined
 * in shell.js and takes three arguments:
 * - a string representation of what is being tested
 * - the expected result
 * - the actual result
 *
 * For example, a test might look like this:
 *
 * var helloWorld = "Hello World";
 *
 * AddTestCase(
 * "var helloWorld = 'Hello World'",   // description of the test
 *  "Hello World",                     // expected result
 *  helloWorld );                      // actual result
 *
 */

///////////////////////////////////////////////////////////////
// add your tests here
  
var obj = new PublicSuperClass();
var obj2 = new PublicSubClass();
//use namespace ns;
var PubInt:PublicInt=obj;
var PubInt5:PublicInt=obj2;
var PubInt2:PublicInt2=obj;
var PubInt6:PublicInt2=obj2;
var PubInt3:PublicInt3=obj;
var PubInt7:PublicInt3=obj2;
/*print(PubInt.MyString());
print(PubInt5.MyString());
print(PubInt2.MyNegInteger());
print(PubInt6.MyNegInteger());
//print(PubInt3.MyString());
//print(PubInt7.MyString());
print(PubInt3.MyNegInteger());
print(PubInt7.MyNegInteger());
print(PubInt3.MyUnsignedInteger());
print(PubInt7.MyUnsignedInteger());
print(obj.MyString());
print(obj2.MyString());
//print(obj.MyNegInteger());
//print(obj2.MyNegInteger());
print(obj.MyUnsignedInteger());
print(obj2.MyUnsignedInteger());
print(obj.PublicInt2::MyString());
print(obj2.PublicInt2::MyString());
print(PubInt3.MyNegativeInteger());
print(PubInt7.MyNegativeInteger());
print(obj.PublicInt3::MyNegativeInteger());
print(obj2.PublicInt3::MyNegativeInteger());*/
//Public Class extends Public class implements a public interface which extends two //interfaces

AddTestCase("Calling a method in public namespace in the public interface implemented  by the superclass through the superclass","Hi!", PubInt.MyString());
AddTestCase("Calling a method in public namespace in the public interface implemented  by the superclass through the subclass","Hi!", PubInt5.MyString());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the superclass",-100, PubInt2.MyNegInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the subclass",-100, PubInt6.MyNegInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the superclass",-100, PubInt3.MyNegInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the subclass",-100, PubInt7.MyNegInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the superclass",100, PubInt3.MyUnsignedInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the subclass",100, PubInt7.MyUnsignedInteger());
AddTestCase("Calling a method in public namespace in the public interface implemented  by the superclass through the superclass","Hi!", obj.MyString());
AddTestCase("Calling a method in public namespace in the public interface implemented  by the superclass through the subclass","Hi!", obj2.MyString());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the superclass",100, obj.MyUnsignedInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the subclass",100, obj2.MyUnsignedInteger());
AddTestCase("Calling a method in public namespace in the public interface implemented  by the superclass through the superclass","Hi!", obj.PublicInt2::MyString());
AddTestCase("Calling a method in public namespace in the public interface implemented  by the superclass through the subclass","Hi!", obj2.PublicInt2::MyString());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the superclass",-100000, PubInt3.MyNegativeInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the subclass",-100000, PubInt7.MyNegativeInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the superclass",-100000, obj.PublicInt3::MyNegativeInteger());
AddTestCase("Calling a method in interface namespace in the public interface implemented  by the superclass through the subclass",-100000, obj2.PublicInt3::MyNegativeInteger());
////////////////////////////////////////////////////////////////

test();       // leave this alone.  this executes the test cases and
              // displays results.
