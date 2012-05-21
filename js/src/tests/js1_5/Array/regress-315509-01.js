/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 315509;
var summary = 'Array.prototype.unshift on Arrays with holes';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var a = [0,1,2,3,4];
delete a[1];

expect = '0,,2,3,4';
actual = a.toString();

reportCompare(expect, actual, summary);

a.unshift('a','b');

expect = 'a,b,0,,2,3,4';
actual = a.toString();
 
reportCompare(expect, actual, summary);
