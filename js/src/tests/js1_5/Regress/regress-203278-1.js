/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 203278;
var summary = 'Don\'t crash in recursive js_MarkGCThing';
var actual = 'FAIL';
var expect = 'PASS';

printBugNumber(BUGNUMBER);
printStatus (summary);

function test1() {}
function test() { test1.call(this); }
test.prototype = new test1();

var length = 512 * 1024 - 1;
var obj = new test();
var first = obj;
for(var i = 0 ; i < length ; i++) {
  obj.next = new test();
  obj = obj.next;
}

actual = 'PASS';

reportCompare(expect, actual, summary);

