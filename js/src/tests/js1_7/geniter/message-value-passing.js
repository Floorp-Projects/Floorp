/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = 326466;
var summary = "Generator value/exception passing";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function gen()
{
  var rv, rv2, rv3, rv4;
  rv = yield 1;

  if (rv)
    rv2 = yield rv;
  else
    rv3 = yield 2;

  try
  {
    if (rv2)
      yield rv2;
    else
      rv3 = yield 3;
  }
  catch (e)
  {
    if (e == 289)
      yield "exception caught";
  }

  yield 5;
}

var failed = false;
var it = gen();

try
{
  if (it.next() != 1)
    throw "failed on 1";

  if (it.send(17) != 17)
    throw "failed on 17";

  if (it.next() != 3)
    throw "failed on 3";

  if (it.throw(289) != "exception caught")
    throw "it.throw(289) failed";

  if (it.next() != 5)
    throw "failed on 5";

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
    throw "missing or incorrect StopIteration";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
