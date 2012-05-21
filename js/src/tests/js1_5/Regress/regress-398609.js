/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 398609;
var summary = 'Test regression from bug 398609';
var actual = 'No Error';
var expect = 'No Error';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function f(m) {
    m = m || Math;
    return x();

    function x() {
      return m.sin(0);
    }
  };

  var r = f();
  if (r !== Math.sin(0))
    throw "Unexpected result";

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
