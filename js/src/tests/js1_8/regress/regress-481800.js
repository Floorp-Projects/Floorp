/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 481800;
var summary = 'TM: Do not assert: (!lhs->isQuad() && !rhs->isQuad()) || (lhs->isQuad() && rhs->isQuad())';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  for (let x of ['', 0, 0, eval]) { y = x } ( function(){} );


  reportCompare(expect, actual, summary);
}
