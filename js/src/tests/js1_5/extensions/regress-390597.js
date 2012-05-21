/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 390597;
var summary = 'watch point + eval-as-setter allows access to dead JSStackFrame';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function exploit() {
    try
    {
    var obj = this, args = null;
    obj.__defineSetter__("evil", eval);
    obj.watch("evil", function() { return "args = arguments;"; });
    obj.evil = null;
    eval("print(args[0]);");
    }
    catch(ex)
    {
      print('Caught ' + ex);
    }
  }
  exploit();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
