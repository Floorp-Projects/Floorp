// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("9.2.1.1 XMLList [[Get]]");

var x =
<>
<alpha attr1="value1">
    <bravo attr2="value2">
        one
        <charlie>two</charlie>
    </bravo>
</alpha>
<alpha attr1="value3">
    <bravo attr2="value4">
        three
        <charlie>four</charlie>
    </bravo>
</alpha>
</>;

// .
correct =
<>
    <bravo attr2="value2">
        one
        <charlie>two</charlie>
    </bravo>
    <bravo attr2="value4">
        three
        <charlie>four</charlie>
    </bravo>
</>;

TEST(1, correct, x.bravo);

correct =
<>
    <charlie>two</charlie>
    <charlie>four</charlie>
</>;

TEST(2, correct, x.bravo.charlie);

// .@
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value3");
TEST(3, correct, x.@attr1);

correct = new XMLList();
correct += new XML("value2");
correct += new XML("value4");
TEST(4, correct, x.bravo.@attr2);

// ..
correct =
<>
    <bravo attr2="value2">
        one
        <charlie>two</charlie>
    </bravo>
    <bravo attr2="value4">
        three
        <charlie>four</charlie>
    </bravo>
</>;

TEST(5, correct, x..bravo);

correct =
<>
    <charlie>two</charlie>
    <charlie>four</charlie>
</>;

TEST(6, correct, x..charlie);

// .@*
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value3");
TEST(7, correct, x.@*);

x =
<alpha attr1="value1" attr2="value2">
    <bravo>
        one
        <charlie>two</charlie>
    </bravo>
</alpha>;

// ..*
correct = <><bravo>one<charlie>two</charlie></bravo>one<charlie>two</charlie>two</>;

XML.prettyPrinting = false;
TEST(8, correct, x..*);
XML.prettyPrinting = true;

x =
<alpha attr1="value1" attr2="value2">
    <bravo attr2="value3">
        one
        <charlie attr3="value4">two</charlie>
    </bravo>
</alpha>;

// ..@
correct = new XMLList();
correct += new XML("value2");
correct += new XML("value3");
TEST(9, correct, x..@attr2)

// ..@*
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
correct += new XML("value3");
correct += new XML("value4");
TEST(10, correct, x..@*);


END();
