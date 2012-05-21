/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Proper quoting of attribute by uneval/toSource';
var BUGNUMBER = 410192;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

expect = '"v"';
actual = uneval(<x a="v"/>.@a);

TEST(1, expect, actual);

END();
