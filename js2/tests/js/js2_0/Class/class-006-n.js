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
* Date: 2001-06-25
*
* SUMMARY: Negative test: a class method may not change the value of this.
* In this test, we don't even instantiate the class...
*/
//-----------------------------------------------------------------------------
var bug = '(none)';
var summary = 'Negative test: a class method may not change the value of this';
var self = this; // capture a reference to the global object


class A
{
  function changeThis()
  {
    this = self;
  }
}



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
  exitFunc ('test');
}
