/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 355474;
var summary = 'Iterating over XML with WAY_TOO_MUCH_GC';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var f = function () { for each (var z in <y/>) { print(z); } };

expect = 'function () { for each (var z in <y/>) { print(z); } }';
actual = f + '';
compareSource(expect, actual, inSection(1) + summary);

expect = actual = 'No Hang';
f();
f();

TEST(2, expect, actual);

END();
