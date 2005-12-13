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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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
/*
 * Date: 2001-08-13
 *
 * SUMMARY: Negative test: class A should not see a global variable
 * if the latter is defined too late. But what exactly is "too late"?
 *
 * In this test, the global variable in question is "objB". To define
 * it "too late", it is not enough to have 'var objB = etc.' as the
 * last line of the script. All var statements in top-level script
 * are "hoisted" and processed FIRST (before any assignments).
 *
 * So we define objB on the last line without any 'var'.
 * The fact that objB is an object variable is immaterial.
 */
//-----------------------------------------------------------------------------
var bug:String = '(none)';
var summary:String = "Negative test: access a variable before it is defined";
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

// creates a new A; but this depends on objB...
var objC = new C;
 
// see Introduction for why we leave out the 'var'
objB = new B;

// WE SHOULD NEVER REACH THIS POINT -
reportFailure(cnFAILURE);
