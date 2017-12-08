// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 329530;
var summary = 'Do not crash when calling toString on a deeply nested function';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

expectExitCode(0);
expectExitCode(5);

var nestingLevel = 1000;

function buildTestFunction(N) {
  var i, funSourceStart = "", funSourceEnd = "";
  for (i=0; i < N; ++i) {
    funSourceStart += "function testFoo() {";
    funSourceEnd += "}";
  }
  return Function(funSourceStart + funSourceEnd);
}

try
{
  var testSource = buildTestFunction(nestingLevel).toString();
  printStatus(testSource.length);
}
catch(ex)
{
  printStatus(ex + '');
}

 
reportCompare(expect, actual, summary);
