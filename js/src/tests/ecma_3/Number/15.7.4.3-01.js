/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = "412068";
var summary = "num.toLocaleString incorrectly accesses its first argument " +
              "even when no first argument has been given";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

try
{
  if ("3" !== 3..toLocaleString())
    throw '"3" should equal 3..toLocaleString()';
  if ("9" !== 9..toLocaleString(8))
    throw 'Number.prototype.toLocaleString should ignore its first argument';
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
