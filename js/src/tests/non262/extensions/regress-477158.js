/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 477158;
var summary = 'Do not assert: v == JSVAL_TRUE || v == JSVAL_FALSE';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  x = 0;
  x = x.prop;
  for (let [] of ['', '']) { switch(x) { default: (function(){}); } };


  reportCompare(expect, actual, summary);
}
