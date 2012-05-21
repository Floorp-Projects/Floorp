// |reftest| skip -- we violate the spec here with our new iterators
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// testcase from Oscar Fogelberg <osfo@home.se>
var BUGNUMBER = 261887;
var summary = 'deleted properties should not be visited by for in';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var count = 0;
var result = "";
var value = "";

var t = new Object();
t.one = "one";
t.two = "two";
t.three = "three";
t.four = "four";
   
for (var prop in t) {
  if (count==1) delete(t.three);
  count++;
  value = value + t[prop];
  result = result + prop;
}
 
expect = 'onetwofour:onetwofour';
actual = value + ':' + result;

reportCompare(expect, actual, summary);
