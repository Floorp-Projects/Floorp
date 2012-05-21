/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Crash in js_GetStringBytes";
var BUGNUMBER = 327897;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);
printStatus('This test runs only in the browser');

if (typeof XMLSerializer != 'undefined')
{
    try
    {
        var a = XMLSerializer;
        a.foo = (function(){}).apply;
        a.__proto__ = <x/>;
        a.foo();
    }
    catch(ex)
    {
        printStatus(ex + '');
    }
}
TEST(1, expect, actual);

END();
