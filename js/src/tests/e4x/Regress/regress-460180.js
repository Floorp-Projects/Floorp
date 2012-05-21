/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Do not crash with if (false || false || <x/>) {}';
var BUGNUMBER = 460180;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

if (false || false || <x/>) { }

TEST(1, expect, actual);

END();
