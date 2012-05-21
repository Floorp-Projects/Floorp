/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 299644;
var summary = 'Arrays with holes';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

actual = (new Array(10).concat()).length;
expect = 10;
reportCompare(expect, actual, '(new Array(10).concat()).length == 10');

var a = new Array(10);
actual = true;
expect = true;
for (var p in a)
{
  actual = false;
  break;
}
reportCompare(expect, actual, 'Array holes are not enumerable');
