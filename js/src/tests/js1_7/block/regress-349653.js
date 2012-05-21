/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349653;
var summary = 'Do not assert: OBJ_GET_CLASS(cx, obj) == &js_ArrayClass';
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
 
  void ({y: true ? [1 for (x in [2])] : 3 })
    reportCompare(expect, actual, summary);

  let (a) true ? [2 for each (z in function(id) { return id })] : 3;
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
