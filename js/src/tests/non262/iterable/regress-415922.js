/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 415922;
var summary = 'Support exception from withing JSNewEnumerateOp on JSENUMERATE_NEXT';
var actual = 'No Error';
var expect = 'Error: its enumeration failed';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function f() { for (k in it) return k }

  if (typeof it == 'undefined')
  {
    print(expect = actual = 'it not defined, test skipped');
  }
  else
  {
    try 
    {
      it.enum_fail = true;
      var r = f();
      actual = 'No exception r: ' + r.toString();
    } 
    catch (e) 
    {
      actual = e + '';
    } 
    finally 
    {
      it.enum_fail = false;
    }
  }
  reportCompare(expect, actual, summary);
}
