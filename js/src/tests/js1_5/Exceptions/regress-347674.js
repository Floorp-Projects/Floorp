/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "347674";
var summary = "ReferenceError thrown when accessing exception bound in a " +
  "catch block in a try block within that catch block";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function foo()
{
  try
  {
    throw "32.9";
  }
  catch (e)
  {
    try
    {
      var errorCode = /^(\d+)\s+.*$/.exec(e)[1];
    }
    catch (e2)
    {
      void("*** internal error: e == " + e + ", e2 == " + e2);
      throw e2;
    }
  }
}

try
{
  try
  {
    foo();
  }
  catch (ex)
  {
    if (!(ex instanceof TypeError))
      throw "Wrong value thrown!\n" +
	"  expected: a TypeError ('32.9' doesn't match the regexp)\n" +
	"  actual: " + ex;
  }
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
