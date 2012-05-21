/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Regression - Do not crash calling __proto__";
var BUGNUMBER = 313080;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    <element/>.__proto__();
    <element/>.function::__proto__();
}
catch(e)
{
    printStatus(e + '');
}
TEST(1, expect, actual);

END();
