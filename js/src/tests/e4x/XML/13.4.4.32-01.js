// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.32-1 - XML replace() by index, text to string");

printBugNumber(291927);

var root = <root>text</root>;

TEST_XML(1, "text", root.child(0));

root.replace(0, "new text");

TEST_XML(2, "new text", root.child(0));
TEST(3, <root>new text</root>, root);

END();
