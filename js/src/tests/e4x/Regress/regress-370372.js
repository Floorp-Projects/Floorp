// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 370372;
var summary = 'with (xmllist) function::name assignments';
var actual = 'No Exception';
var expect = 'No Exception';

printBugNumber(BUGNUMBER);
START(summary);

var tests = [{ v: <></>, expect: "" },
             { v: <><a/></>, expect: "" },
             { v: <><a/><b/></>, expect: "<a/>\n<b/>" }];

function do_one(x, expect)
{
    x.function::f = Math.sin;
    with (x) {
        function::toString = function::f = function() { return "test"; };
    }

    assertEq(String(x), expect, "didn't implement ToString(XMLList) correctly: " + x.toXMLString());

    if (x.f() !== "test")
        throw "Failed to set set function f";
}


for (var i  = 0; i != tests.length; ++i)
    do_one(tests[i].v, tests[i].expect);

TEST(1, expect, actual);
END();
