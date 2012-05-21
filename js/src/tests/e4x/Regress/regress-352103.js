/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER     = "352103";
var summary = "<??> XML initializer should generate a SyntaxError";
var actual, expect;

printBugNumber(BUGNUMBER);
START(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

try
{
  try
  {
    eval("var x = <??>;"); // force non-compile-time exception
    throw "No SyntaxError thrown!";
  }
  catch (e)
  {
    if (!(e instanceof SyntaxError))
      throw "Unexpected exception: " + e;
  }
}
catch (ex)
{
  failed = ex;
}

expect = false;
actual = failed;

TEST(1, expect, actual);
