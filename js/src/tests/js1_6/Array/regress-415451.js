/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 415451;
var summary = 'indexOf/lastIndexOf behavior';

var expected = "3,0,3,3,3,-1,-1";
results = [];
var a = [1,2,3,1];
for (var i=-1; i < a.length+2; i++)
  results.push(a.indexOf(1,i));
var actual = String(results);
reportCompare(expected, actual, "indexOf");

results = [];
var expected = "3,0,0,0,3,3,3";
for (var i=-1; i < a.length+2; i++)
  results.push(a.lastIndexOf(1,i));
var actual = String(results);
reportCompare(expected, actual, "lastIndexOf");

