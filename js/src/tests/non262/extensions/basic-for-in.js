/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "346582";
var summary = "Basic support for iterable objects and for-in";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

var iterable = { persistedProp: 17 };

try
{
  // nothing unusual so far -- verify basic properties
  for (var i in iterable)
  {
    if (i != "persistedProp")
      throw "no persistedProp!";
    if (iterable[i] != 17)
      throw "iterable[\"persistedProp\"] == 17";
  }

  if (iterable.persistedProp != 17)
    throw "iterable.persistedProp not persisted!";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
