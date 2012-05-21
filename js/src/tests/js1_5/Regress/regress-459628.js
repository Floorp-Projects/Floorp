/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 459628;
var summary = 'Do not assert: JSVAL_IS_VOID(STOBJ_GET_SLOT(obj, map->freeslot))';
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
 
  jit(true);

(function() { 
  for (var odjoff = 0; odjoff < 4; ++odjoff) { 
    new Date()[0] = 3; 
  } 
})();

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
