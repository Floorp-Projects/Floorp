// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 445818;
var summary = 'Do not crash with threads';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

printBugNumber(BUGNUMBER);
printStatus (summary);
 
if (typeof scatter == 'undefined')
{
  print(expect = actual = 'Test skipped. scatter not defined');
}
else
{
  var array = [{}, {}, {}, {}];

  function test() {
    for (var i = 0; i != 42*42*42; ++i) {
      var obj = array[i % array.length];
      obj["a"+i] = 1;
      var tmp = {}; 
      tmp["a"+i] = 2; 
    }
  }

  scatter([test, test, test, test]);
}

reportCompare(expect, actual, summary);
