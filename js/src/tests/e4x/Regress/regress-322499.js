/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Do not define AnyName";
var BUGNUMBER = 322499;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

expect = 'ReferenceError';

try
{
    AnyName;
}
catch(ex)
{
    actual = ex.name;
}

TEST(1, expect, actual);

try
{
    *;
}
catch(ex)
{
    actual = ex.name;
}
TEST(2, expect, actual);

END();
