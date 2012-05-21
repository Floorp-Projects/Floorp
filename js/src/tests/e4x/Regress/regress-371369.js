/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 371369;
var summary = 'delete xml.function::name does not work';
var actual = 'No Exception';
var expect = 'No Exception';

printBugNumber(BUGNUMBER);
START(summary);

var xml = <a/>;
xml.function::something = function() { };

delete xml.function::something;

try {
    xml.something();
    throw "Function can be called after delete";
} catch(e) {
    if (!(e instanceof TypeError))
        throw "Unexpected exception: " + e;
}

TEST(1, expect, actual);
END();
