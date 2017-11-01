/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352372;
var summary = 'Do not assert eval("setter/*...")';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'ReferenceError: setter is not defined';
  try
  {
    eval("setter/*\n*/;");
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, 'eval("setter/*\n*/;")');

  try
  {
    eval("setter/*\n*/g");
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, 'eval("setter/*\n*/g")');

  try
  {
    eval("setter/*\n*/ ;");
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, 'eval("setter/*\n*/ ;")');

  try
  {
    eval("setter/*\n*/ g");
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, 'eval("setter/*\n*/ g")');
}
