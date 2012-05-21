/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 356238;
var summary = 'bug 356238';
var actual = 'No Duplicate';
var expect = 'No Duplicate';

printBugNumber(BUGNUMBER);
START(summary);

var xml = <xml><child><a/><b/></child></xml>;
var child = xml.child[0];

try {
    child.insertChildAfter(null, xml.child);
    actual = "insertChildAfter succeeded when it should throw an exception";
}
catch (e) {
}

if (child.a[0] === child.a[1])
    actual = 'Duplicate detected';

TEST(1, expect, actual);
END();
