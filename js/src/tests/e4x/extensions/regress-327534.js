/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "uneval on E4X gives Error: xml is not a function";
var BUGNUMBER = 327534;
var actual = '';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    uneval(<x/>);
    actual = 'No Error';
}
catch(ex)
{
    printStatus(ex);
    actual = ex + '';
}
TEST(1, expect, actual);

END();
