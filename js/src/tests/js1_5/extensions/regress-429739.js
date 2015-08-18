/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 429739;
var summary = 'Do not assert: OBJ_GET_CLASS(cx, obj)->flags & JSCLASS_HAS_PRIVATE';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var actual;
  try
  {
    var o = { __noSuchMethod__: Function }; 
    actual = (new o.y) + '';
    throw new Error("didn't throw, produced a value");
  }
  catch(ex)
  {
    actual = ex;
  }

  reportCompare("TypeError", actual.name, "bad error name");
  reportCompare(true, /is not a constructor/.test(actual), summary);

  exitFunc ('test');
}
