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
* SUMMARY: Negative test: class A should not see a global object variable
* if the latter is defined too late (before the first instance of A is defined)
*/
//-----------------------------------------------------------------------------
var bug:String = '(none)';
var summary:String = "Negative test: accessing object var before it's defined";
var cnFAILURE:String = 'Expected an exception to be thrown, but none was -';


class A
{
  var obj:Object;

  constructor function A()
  {
    obj = objB; // defined below (BUT TOO LATE)
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

var objC = new C; // creates a new A; but this depends on objB...
var objB = new B;

// WE SHOULD NEVER REACH THIS POINT -
reportFailure(cnFAILURE);
