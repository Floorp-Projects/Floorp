/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER     = "350206";
var summary = "Order of destructuring, destructuring in the presence of " +
              "exceptions";
var actual, expect;

printBugNumber(BUGNUMBER);
START(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = "No crash";

try
{
  var t1 = <ns2:t1 xmlns:ns2="http://bar/ns2"/>;
  var n2 = new Namespace("http://ns2");
  t1.@n2::a1 = "a1 from ns2";

  t1.toXMLString();
}
catch (ex)
{
  failed = ex;
}

expect = "No crash";
actual = failed;

TEST(1, expect, actual);
