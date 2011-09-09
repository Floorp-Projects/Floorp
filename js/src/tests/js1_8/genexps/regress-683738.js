/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dave Herman
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


//-----------------------------------------------------------------------------
var BUGNUMBER = 683738;
var summary = 'return with argument and lazy generator detection';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = "generator function foo returns a value";
  try
  {
    actual = 'No Error';
    eval("function foo(x) { if (x) { return this; } else { yield 3; } }");
  }
  catch(ex)
  {
    actual = ex.message;
  }
  reportCompare(expect, actual, summary + ": 1");

  expect = "generator function foo returns a value";
  try
  {
    actual = 'No Error';
    eval("function foo(x) { if (x) { yield 3; } else { return this; } }");
  }
  catch(ex)
  {
    actual = ex.message;
  }
  reportCompare(expect, actual, summary + ": 2");

  expect = "generator function foo returns a value";
  try
  {
    actual = 'No Error';
    eval("function foo(x) { if (x) { return this; } else { (yield 3); } }");
  }
  catch(ex)
  {
    actual = ex.message;
  }
  reportCompare(expect, actual, summary + ": 3");

  expect = "generator function foo returns a value";
  try
  {
    actual = 'No Error';
    eval("function foo(x) { if (x) { (yield 3); } else { return this; } }");
  }
  catch(ex)
  {
    actual = ex.message;
  }
  reportCompare(expect, actual, summary + ": 4");

}
