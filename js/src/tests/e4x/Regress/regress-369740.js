/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 369740;
var summary = 'generic code for function::';
var actual = 'No Exception';
var expect = 'No Exception';

printBugNumber(BUGNUMBER);
START(summary);

actual = expect = Math.function::sin + '';
TEST(1, expect, actual);

var x = <xml/>;
x.function::toString = function(){return "moo"};
actual = x + '';
expect = '';
TEST(2, expect, actual);

x = <><a/><b/></>;

expect = "<a/>\n<b/>";
try
{
    with (x) {
        function::toString = function() { return "test"; }
    }

    actual = x + '';
}
catch(ex)
{
    actual = ex + '';
}
TEST(3, expect, actual);

// test for regression caused by initial patch
expect = actual = 'No Crash';
const xhtmlNS = null;
this[xhtmlNS] = {};
TEST(4, expect, actual);


END();
