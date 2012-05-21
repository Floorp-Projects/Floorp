/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Regression - appending elements crashes mozilla";
var BUGNUMBER = 309897;
var actual = "No Crash";
var expect = "No Crash";

printBugNumber(BUGNUMBER);
START(summary);

function crash()
{
    try
    {
        var top = <top/>;
        for (var i = 0; i < 1000; i++)
        {
            top.stuff += <stuff>stuff</stuff>;
        }
    }
    catch(e)
    {
        printStatus("Exception: " + e);
    }
}

crash();

TEST(1, expect, actual);

END();
