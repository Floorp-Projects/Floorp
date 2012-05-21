/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.37 - XML text()");

TEST(1, true, XML.prototype.hasOwnProperty("text"));

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST_XML(2, "one", x.bravo.text());

correct = new XMLList();
correct += new XML("one");
correct += new XML("two");
TEST(3, correct, x..bravo.text());   
TEST_XML(4, "", x.charlie.text());
TEST_XML(5, "", x.foobar.text());
TEST_XML(6, "one", x.*.text());

END();
