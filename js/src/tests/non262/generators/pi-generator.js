/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var summary = "A (slow) generator of pi";
var actual, expect;

printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function* pi()
{
  var val = 0;
  var curr = 1;
  var isNeg = false;
  while (true)
  {
    if (isNeg)
      yield val -= 4/curr;
    else
      yield val += 4/curr;
    curr += 2;
    isNeg = !isNeg;
  }
}

var failed = false;
var it = pi();

var vals =
  [4,
   4 - 4/3,
   4 - 4/3 + 4/5,
   4 - 4/3 + 4/5 - 4/7];

try
{
  for (var i = 0, sz = vals.length; i < sz; i++)
    if (it.next().value != vals[i])
      throw vals[i];
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
