/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 244619;
var summary = 'Don\'t Crash';
var actual = 'Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

function f1()
{
  var o = new Object();
  eval.call(o, "var a = 'vodka'"); // <- CRASH !!!
}

// Rhino does not allow indirect eval calls
try
{
  f1();
}
catch(e)
{
}

actual = 'No Crash';
 
reportCompare(expect, actual, summary);
