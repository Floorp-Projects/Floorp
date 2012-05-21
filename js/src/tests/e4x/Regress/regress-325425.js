/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 325425;
var summary = 'jsxml.c: Bad assumptions about js_ConstructObject';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    QName = function() { };
    <xml/>.elements("");
}
catch(ex)
{
    printStatus(ex + '');
}
TEST(1, expect, actual);

END();
