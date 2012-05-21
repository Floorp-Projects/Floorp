/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 457521;
var summary = 'Do not crash @ js_DecompileValueGenerator';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  this.__defineSetter__("x", [,,,].map);
  this.watch("x", (new Function("var y, eval")));
  x = true;
}
catch(ex)
{
}
reportCompare(expect, actual, summary);
