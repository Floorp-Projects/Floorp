/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 293782;
var summary = 'Local variables should not be enumerable properties of the function';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function f()
{
  var x,y
    }

var p;
actual = '';

for (p in f)
{
  actual += p + ',';
}
expect = '';
 
reportCompare(expect, actual, summary);
