/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
var actual = '';
var expect = '';

//-------  Comment #162  From  Gary Kwong

printBugNumber(BUGNUMBER);
printStatus (summary);

// Assertion failure: !OBJ_GET_CLASS(cx, proto)->getObjectOps, at ../jsobj.cpp:2030

jit(true);
this.__defineGetter__("x3", Function);
parseInt = x3;
parseInt.prototype = [];
for (var z = 0; z < 4; ++z) { new parseInt() }
jit(false);

reportCompare(expect, actual, summary);
