/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465686;
var summary = 'Do not crash @ tiny_free_list_add_ptr';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  for (let b of [eval, eval, 4, 4]) { 
      ++b; 
      for (b of [(void 0), (void 0), (void 0), 3, (void 0), 3]) { 
          b ^= b; 
          for (var c of [1/0]) {
          }
      } 
  }

  reportCompare(expect, actual, summary);
}
