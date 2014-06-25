/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452742;
var summary = 'Do not do overzealous eval inside function optimization in BindNameToSlot';
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
 
  expect = actual = 'No Error';

  var obj = { x: -100 };

  function a(x)
  {
    var orig_x = x;
    var orig_obj_x = obj.x;

    with (obj) { eval("x = x + 10"); }

    if (x !== orig_x)
      throw "Unexpected mutation of x: " + x;
    if (obj.x !== orig_obj_x + 10)
      throw "Unexpected mutation of obj.x: " + obj.x;
  }

  try
  {
    a(0);
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
