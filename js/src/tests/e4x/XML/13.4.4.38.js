// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.38 - XML toString()");

TEST(1, true, XML.prototype.hasOwnProperty("toString"));

XML.prettyPrinting = false;

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(2, "one", x.bravo.toString());
TEST(3, "<bravo>one</bravo><bravo>two</bravo>", x..bravo.toString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie/>
</alpha>;

TEST(4, "", x.charlie.toString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(5, "<charlie><bravo>two</bravo></charlie>", x.charlie.toString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        two
        <bravo/>
    </charlie>
</alpha>;

TEST(5, "<charlie>two<bravo/></charlie>", x.charlie.toString());

x =
<alpha>
    <bravo></bravo>
    <bravo/>
</alpha>;

TEST(6, "<bravo/><bravo/>", x.bravo.toString());

x =
<alpha>
    <bravo>one<charlie/></bravo>
    <bravo>two<charlie/></bravo>
</alpha>;

TEST(7, "<bravo>one<charlie/></bravo><bravo>two<charlie/></bravo>", x.bravo.toString());

END();
