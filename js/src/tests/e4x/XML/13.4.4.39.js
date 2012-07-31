// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.39 - XML toXMLString");

TEST(1, true, XML.prototype.hasOwnProperty("toXMLString"));

XML.prettyPrinting = false;

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(2, "<bravo>one</bravo>", x.bravo.toXMLString());
TEST(3, "<bravo>one</bravo><bravo>two</bravo>", x..bravo.toXMLString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie/>
</alpha>;

TEST(4, "<charlie/>", x.charlie.toXMLString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(5, "<charlie><bravo>two</bravo></charlie>", x.charlie.toXMLString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        two
        <bravo/>
    </charlie>
</alpha>;

TEST(6, "<charlie>two<bravo/></charlie>", x.charlie.toXMLString());

x =
<alpha>
    <bravo></bravo>
    <bravo/>
</alpha>;

TEST(7, "<bravo/><bravo/>", x.bravo.toXMLString());

x =
<alpha>
    <bravo>one<charlie/></bravo>
    <bravo>two<charlie/></bravo>
</alpha>;

TEST(8, "<bravo>one<charlie/></bravo><bravo>two<charlie/></bravo>", x.bravo.toXMLString());
  
XML.prettyPrinting = true;

x =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

copy = x.bravo.copy();

TEST(9, "<bravo>one</bravo>", copy.toXMLString());

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(10, "String contains value one from bravo", "String contains value " + x.bravo + " from bravo");

END();
