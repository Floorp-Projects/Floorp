/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 474935;
var summary = 'Do not assert: !ti->typeMap.matches(ti_other->typeMap)';
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
 

  var a = ["", 0, 0, 0, 0, 0, "", "", 0, "", 0, ""];
  var i = 0;
  var g = 0;
  for each (let e in a) {
      "" + [e];
      if (i == 3 || i == 7) {
        for each (g in [1]) {
          }
      }
      ++i;
    }


  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
