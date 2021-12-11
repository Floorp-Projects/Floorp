// |reftest| skip-if(Android) silentfail skip -- disabled pending bug 657444
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 338121;
var summary = 'Issues with JS_ARENA_ALLOCATE_CAST';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

expectExitCode(0);
expectExitCode(5);

var fe="vv";

for (i=0; i<24; i++)
  fe += fe;

var fu=new Function(
  fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe,
  fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe,
  "done"
  );

//alert("fu="+fu);
//print("fu="+fu);
var fuout = 'fu=' + fu;

print('Done');

reportCompare(expect, actual, summary);
