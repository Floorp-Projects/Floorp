/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 470187;
var summary = 'Do not assert: ATOM_IS_STRING(atom)';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  for (var j=0;j<3;++j) ({valueOf: function(){return 2}}) - [];

  reportCompare(expect, actual, summary);
}
