/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465453;
var summary = 'Do not convert (undefined) to "undefined"';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = "true,,true,true,,,,,";

  var out = [];
  for (var e of [(new Boolean(true)),
                 (void 0),
                 (new Boolean(true)),
                 (new Boolean(true)),
                 (void 0),
                 (void 0),
                 "",
                 "",
                 (void 0)])
             out.push(e);
  print(actual = out.toString());

  reportCompare(expect, actual, summary);
}
