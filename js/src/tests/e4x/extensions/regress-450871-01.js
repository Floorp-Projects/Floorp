/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Do not crash: __proto__ = <x/>; <x/>.lastIndexOf(this, false)';
var BUGNUMBER = 450871;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

if (typeof window == 'object')
{
    actual = expect = 'Test skipped for browser based tests due destruction of the prototype';
}
else
{
    try
    {
        __proto__ = <x/>; 
        <x/>.lastIndexOf(this, false);
    }
    catch(ex)
    {
    }
}

TEST(1, expect, actual);

END();
