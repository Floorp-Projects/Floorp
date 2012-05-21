/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 382532;
var summary = 'instanceof,... broken by use of |prototype| in heavyweight constructor';
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
 
  var prototype;

  function Bug() {
    var func = function () { x; };
    prototype;
  }

  expect = true;
  actual = (new Bug instanceof Bug);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
