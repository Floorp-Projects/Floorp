/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355829;
var summary = 'js_ValueToObject should return the original object if OBJ_DEFAULT_VALUE returns a primitive value';
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

  expect = actual = 'Good conversion';

  var primitiveValues = [
    true, false, 0, 1, -2, 0.1, -2e100, 0/0, 1/0, -1/1,  "", "xxx",
    undefined, null
    ];

  for (var i = 0; i != primitiveValues.length; ++i) {
    var v = primitiveValues[i];
    var obj = { valueOf: function() { return v; } };
    var obj2 = Object(obj);
    if (obj !== obj2)
      actual = "Bad conversion for '"+v + "'";
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
