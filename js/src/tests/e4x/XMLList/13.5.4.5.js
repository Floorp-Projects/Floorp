/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.5 - XMLList children()");

TEST(1, true, XMLList.prototype.hasOwnProperty("children"));

// Test with XMLList of size 0
x = new XMLList()
TEST(2, "xml", typeof(x.children()));
TEST_XML(3, "", x.children());

// Test with XMLList of size 1
x += <alpha>one<bravo>two</bravo></alpha>;   
TEST(4, "xml", typeof(x.children()));

correct = <>one<bravo>two</bravo></>;
TEST(5, correct, x.children());

// Test with XMLList of size > 1
x += <charlie><bravo>three</bravo></charlie>;
TEST(6, "xml", typeof(x.children()));

correct = <>one<bravo>two</bravo><bravo>three</bravo></>;
TEST(7, correct, x.children());

// Test no children
x = new XMLList();
x += <alpha></alpha>;
x += <bravo></bravo>;
TEST(8, "xml", typeof(x.children()));
TEST_XML(9, "", x.children());

//get all grandchildren of the order that have the name price

order =
<order>
    <customer>
        <name>John Smith</name>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
</order>;

correct= <price>1299.99</price> + <price>399.99</price>;

TEST(10, correct, order.children().price);

END();
