/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "E4X - Do not crash with XMLList filters";
var BUGNUMBER = 301596;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    <xml/>.(@a == 1);
    throw 5;
}
catch (e)
{
}

TEST(1, expect, actual);

END();
