/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 311792;
var summary = 'Root Array.prototype methods';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

function index_getter()
{
  gc();
  return 100;
}

var a = [0, 1];
a.__defineGetter__(0, index_getter);

uneval(a.slice(0, 1));
 
reportCompare(expect, actual, summary);
