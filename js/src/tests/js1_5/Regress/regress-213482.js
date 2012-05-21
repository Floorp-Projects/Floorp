/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 213482;
var summary = 'Do not crash watching property when watcher sets property';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var testobj = {value: 'foo'};

function watched (a, b, c) {
  testobj.value = (new Date()).getTime();
}

function setTest() {
  testobj.value = 'b';
}

testobj.watch("value", watched);

setTest();

reportCompare(expect, actual, summary);
