/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 370016;
var summary = 'with (nonxmlobj) function::';
var actual = 'No Exception';
var expect = 'No Exception';

printBugNumber(BUGNUMBER);
START(summary);

with (Math) print(function::sin(0))

TEST(1, expect, actual);
END();
