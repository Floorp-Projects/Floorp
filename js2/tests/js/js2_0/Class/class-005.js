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
* Date: 2001-06-24
*
* SUMMARY: Should not get 'duplicate class definition' warning
*          if the second definition is commented out -
*/
//-------------------------------------------------------------------------------------------------
var bug = '(none)';
var summary = "Don't give 'duplicate class definition' warning if 2nd definition is commented out";


class A {}  // class A{}
var a = new A;


class B {}  /* class B {} */
var b = new B;


class C
{
}
var c = new C;
/*********************************************************
class C
{
}
var c = new C;
*********************************************************/


class D
{
  function m() {return 1111;}
}
/*********************************************************
class D
{
  function m() {return 22222;}
}
var d = new D;
*********************************************************/

var d = new D;



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
  exitFunc ('test');
}
