/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START('XML("") should create empty text node');
printBugNumber(271545);

// Check that text node should ignore any attempt to add a child to it


var x;

x = new XML();
x.a = "foo";
TEST_XML(1, "", x);

x = new XML("");
x.a = "foo";
TEST_XML(2, "", x);

x = new XML(null);
x.a = "foo";
TEST_XML(3, "", x);

x = new XML(undefined);
x.a = "foo";
TEST_XML(4, "", x);

var textNodeContent = "some arbitrary text without XML markup";

x = new XML(textNodeContent);
x.a = "foo";
TEST_XML(5, textNodeContent, x);

END();
