/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.12 - XMLList hasComplexContent()");

TEST(1, true, XMLList.prototype.hasOwnProperty("hasComplexContent"));

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

TEST(2, true, x.hasComplexContent());
TEST(3, false, x.bravo.hasComplexContent());
TEST(4, true, x.charlie.hasComplexContent());
TEST(5, false, x.delta.hasComplexContent());
TEST(6, false, x.foxtrot.hasComplexContent());
TEST(7, false, x.golf.hasComplexContent());
TEST(8, false, x.hotel.hasComplexContent());
TEST(9, false, x.@attr1.hasComplexContent());
TEST(10, false, x.bravo.child(0).hasComplexContent());
TEST(11, true, x.india.hasComplexContent());

// More than one element is complex if one or more things in the list are elements.
x =
<>
<alpha>one</alpha>
<bravo>two</bravo>
</>;

TEST(12, true, x.hasComplexContent());

x =
<root>
    one
    <alpha>one</alpha>
</root>;

TEST(13, true, x.*.hasComplexContent());

x =
<root attr1="value1" attr2="value2">
    one
</root>;

TEST(14, false, x.@*.hasComplexContent());
  
END();
