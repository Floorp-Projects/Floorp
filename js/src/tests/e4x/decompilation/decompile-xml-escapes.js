// |reftest| fails
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER     = "352285";
var summary = "Decompiler escapes line breaks/backslashes in E4X literals";
var actual, expect;

printBugNumber(BUGNUMBER);
START(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function assertCorrectDecompilation(xmlInitializer)
{
  var func = new Function("return " + xmlInitializer);
  var funcStr = func.toString();
  if (funcStr.indexOf(xmlInitializer) < 0)
    throw "'" + funcStr + "' does not contain '" + xmlInitializer + "'!";
}

try
{
  assertCorrectDecompilation("<![CDATA[\\\\]]>");
  assertCorrectDecompilation("<![CDATA[\n]]>");
  assertCorrectDecompilation("<![CDATA[foo\nbar\nbaz]]>");
  assertCorrectDecompilation("<!--f b\nc\n-->");
  assertCorrectDecompilation("<?f b\n\nc\nc?>");
}
catch (ex)
{
  failed = ex;
}

expect = false;
actual = failed;

TEST(1, expect, actual);
