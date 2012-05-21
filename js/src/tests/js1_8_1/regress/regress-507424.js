/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 507424;
var summary = 'TM: assert with regexp literal inside closure'
var actual = '';
var expect = 'do not crash';


//-----------------------------------------------------------------------------
start_test();
jit(true);

(new Function("'a'.replace(/a/,function(x){return(/x/ for each(y in[x]))})"))();

jit(false);
actual = 'do not crash'
finish_test();
//-----------------------------------------------------------------------------

function start_test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
}

function finish_test()
{
  reportCompare(expect, actual, summary);
  exitFunc ('test');
}
