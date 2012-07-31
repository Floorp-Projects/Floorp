// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "11.1.5 XMLList Initialiser Don't Crash with empty Initializer";
var BUGNUMBER = 290499;
var actual = "No Crash";
var expect = "No Crash";

printBugNumber(BUGNUMBER);
START(summary);

var emptyList = <></>;

TEST(1, expect, actual);

END();
