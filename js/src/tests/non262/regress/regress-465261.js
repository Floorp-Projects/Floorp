/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465261;
var summary = 'TM: Do not assert: ';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 

  for (let z = 0; z < 2; ++z) { 
    for (let x of [0, true, (void 0), 0, (void 0)]) { 
        if(x){} 
    } 
  };


  reportCompare(expect, actual, summary);
}
