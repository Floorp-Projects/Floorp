/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "gen.close(); gen.throw(ex) throws ex forever";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function gen()
{
  var x = 5, y = 7;
  var z = x + y;
  yield z;
}

var failed = false;
var it = gen();

try
{
  it.close();

  // throw on closed generators just rethrows
  var doThrow = true;
  var thrown = "foobar";
  try
  {
    it.throw(thrown);
  }
  catch (e)
  {
    if (e === thrown)
      doThrow = false;
  }

  if (doThrow)
    throw "it.throw(\"" + thrown + "\") failed";

  // you can throw stuff at a closed generator forever
  doThrow = true;
  thrown = "sparky";
  try
  {
    it.throw(thrown);
  }
  catch (e)
  {
    if (e === thrown)
      doThrow = false;
  }

  if (doThrow)
    throw "it.throw(\"" + thrown + "\") failed";

  // don't execute a yield -- the uncaught exception
  // exhausted the generator
  var stopPassed = false;
  try
  {
    it.next();
  }
  catch (e)
  {
    if (e === StopIteration)
      stopPassed = true;
  }

  if (!stopPassed)
    throw "invalid or incorrect StopIteration";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
