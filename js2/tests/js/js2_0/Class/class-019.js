/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an
* "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 2001-08-13
*
* SUMMARY: A class A should not see a global variable if the latter
* is defined too late. But what exactly is "too late"?
*
* In this test, the global variable in question is "objB". To define
* it "too late", it would not be enough to have 'var objB = etc.'
* as the last line of the script. All top-level var statements
* are "hoisted" and processed FIRST (before any assignments).
*
* So we define objB on the last line with a 'var'. That should make 
* the test pass. The fact that objB is an object variable is immaterial.
* 
* This test is just like test 'class-018-n.js', except on this point.
* Class-018-n.js omitted the 'var', to generate an exception - 
*/
//-----------------------------------------------------------------------------
var bug:String = '(none)';
var summary:String = "Accessing a global variable defined at end of script";


class A
{
  var obj:Object;

  constructor function A()
  {
    obj = objB; // global variable defined below
  }
}


class B
{
  var obj:Object;

  constructor function B()
  {
    obj = {};
  }
}


class C
{
  var obj:Object;

  constructor function C()
  {
    obj = new A;
  }
}



printBugNumber (bug);
printStatus (summary);

// creates a new A; which depends on objB...
var objC = new C;
 
// having the var statement should make the test pass; see Introduction
var objB = new B;
