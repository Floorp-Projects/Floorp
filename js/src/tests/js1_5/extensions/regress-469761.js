/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 469761;
var summary = 'TM: Do not assert: STOBJ_GET_SLOT(callee_obj, JSSLOT_PRIVATE).isInt32()';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 

  var o = { __proto__: function(){} };
  for (var j = 0; j < 3; ++j) { try { o.call(3); } catch (e) { } }


  reportCompare(expect, actual, summary);
}
