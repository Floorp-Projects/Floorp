/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.13 - XMLList hasSimpleContent()");

TEST(1, true, XMLList.prototype.hasOwnProperty("hasSimpleContent"));

// One element should be same as XML case
x =
<>
<alpha attr1="value1">
    <bravo>one</bravo>
    <charlie>
        two
        three
        <echo>four</echo>
    </charlie>
    <delta />
    <foxtrot attr2="value2">five</foxtrot>
    <golf attr3="value3" />
    <hotel>
        six
        seven
    </hotel>
    <india><juliet /></india>
</alpha>;
</>;

TEST(2, false, x.hasSimpleContent());
TEST(3, true, x.bravo.hasSimpleContent());
TEST(4, false, x.charlie.hasSimpleContent());
TEST(5, true, x.delta.hasSimpleContent());
TEST(6, true, x.foxtrot.hasSimpleContent());
TEST(7, true, x.golf.hasSimpleContent());
TEST(8, true, x.hotel.hasSimpleContent());
TEST(9, true, x.@attr1.hasSimpleContent());
TEST(10, true, x.bravo.child(0).hasSimpleContent());
TEST(11, false, x.india.hasSimpleContent());

// More than one element is complex if one or more things in the list are elements.
x =
<>
<alpha>one</alpha>
<bravo>two</bravo>
</>;

TEST(12, false, x.hasSimpleContent());

x =
<root>
    one
    <alpha>one</alpha>
</root>;

TEST(13, false, x.*.hasSimpleContent());

x =
<root attr1="value1" attr2="value2">
    one
</root>;

TEST(14, true, x.@*.hasSimpleContent());

END();
