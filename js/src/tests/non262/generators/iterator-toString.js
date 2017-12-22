/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var summary = "gen.toString() omitting 'yield' from value";
var actual, expect;

printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function* gen()
{
  yield 17;
}

try
{
  var str = gen.toString();
  var index = str.search(/yield/);

  if (index < 0)
    throw "yield not found in str: " + str;
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
