// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.12 - XML descendants");

TEST(1, true, XML.prototype.hasOwnProperty("descendants"));

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        two
        <bravo>three</bravo>
    </charlie>
</alpha>;

TEST(2, <bravo>three</bravo>, x.charlie.descendants("bravo"));
TEST(3, <><bravo>one</bravo><bravo>three</bravo></>, x.descendants("bravo"));

// Test *
correct = <><bravo>one</bravo>one<charlie>two<bravo>three</bravo></charlie>two<bravo>three</bravo>three</>;

XML.prettyPrinting = false;
TEST(4, correct, x.descendants("*"));
TEST(5, correct, x.descendants());
XML.prettyPrinting = true;

END();
