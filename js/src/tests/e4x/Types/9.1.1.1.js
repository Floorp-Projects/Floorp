// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START('9.1.1.1 XML [[Get]]');

var x =
<alpha attr1="value1" attr2="value2">
    <bravo>
        one
        <charlie>two</charlie>
    </bravo>
</alpha>;

// .
TEST(1, <bravo>one<charlie>two</charlie></bravo>, x.bravo);
TEST(2, <charlie>two</charlie>, x.bravo.charlie);
TEST(3, <charlie>two</charlie>, x.bravo.charlie.parent().charlie);

// .*
var correct = <>one<charlie>two</charlie></>;
TEST(4, correct, x.bravo.*);
TEST_XML(5, "two", x.bravo.charlie.*);
TEST(6, <bravo>one<charlie>two</charlie></bravo>, x.*[0]);

// .@
TEST_XML(7, "value1", x.@attr1);
TEST_XML(8, "value2", x.@attr2);

// ..
TEST(9, <bravo>one<charlie>two</charlie></bravo>, x..bravo);
TEST(10, <charlie>two</charlie>, x..charlie);

// .@*
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
TEST(11, correct, x.@*);

x =
<alpha attr1="value1" attr2="value2">
    <bravo>
        one
        <charlie>two</charlie>
    </bravo>
</alpha>;

// ..*
XML.prettyPrinting = false;
correct = <><bravo>one<charlie>two</charlie></bravo>one<charlie>two</charlie>two</>;
TEST(12, correct, x..*);
XML.prettyPrinting = true;

x =
<alpha attr1="value1" attr2="value2">
    <bravo attr3="value3">
        one
        <charlie attr3="value4">two</charlie>
    </bravo>
</alpha>;

// ..@
correct = new XMLList();
correct += new XML("value3");
correct += new XML("value4");
TEST(13, correct, x..@attr3)

// ..@*
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
correct += new XML("value3");
correct += new XML("value4");
TEST(14, correct, x..@*);

// Check reserved words
x =
<alpha>
    <prototype>one</prototype>
</alpha>;

TEST(15, <prototype>one</prototype>, x.prototype);

// Check method names
x =
<alpha>
    <name>one</name>
    <toString>two</toString>
</alpha>;

TEST(16, <name>one</name>, x.name);
TEST(17, QName("alpha"), x.name());
TEST(18, <toString>two</toString>, x.toString);
TEST(19, x.toXMLString(), x.toString());

// Test super-expandos
x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.bravo.charlie.delta = <delta>two</delta>;
TEST(20, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.bravo.charlie.delta = "two";
TEST(21, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <echo>two</echo>
        </charlie>
     </bravo>
</alpha>;

x.bravo.charlie.delta = <echo>two</echo>;
TEST(22, correct, x);

// Also ADD with *, children() and child()
x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.*.charlie.delta = <delta>two</delta>;
TEST(23, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.children().charlie.delta = <delta>two</delta>;
TEST(24, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        one
        <charlie>
            <delta>two</delta>
        </charlie>
     </bravo>
</alpha>;

x.child("bravo").charlie.delta = <delta>two</delta>;
TEST(25, correct, x);

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>one</bravo>
    <newChild>
        <charlie>
            <delta>two</delta>
        </charlie>
     </newChild>
</alpha>;

x.child("newChild").charlie.delta = <delta>two</delta>;
TEST(26, correct, x);

// These should fail because the XMLList is size > 1
x =
<alpha>
    <bravo>one</bravo>
    <bravo>two</bravo>
</alpha>;

try {
    x.*.charlie.delta = "more";
    SHOULD_THROW(27);
} catch (ex) {
    TEST(27, "TypeError", ex.name);
}

x =
<alpha>
    <bravo>one</bravo>
    <bravo>two</bravo>
</alpha>;

try {
    x.children().charlie.delta = "more";
    SHOULD_THROW(28);
} catch (ex) {
    TEST(28, "TypeError", ex.name);
}

x =
<alpha>
    <bravo>one</bravo>
    <bravo>two</bravo>
</alpha>;

try {
    x.child("bravo").charlie.delta = "more";
    SHOULD_THROW(29);
} catch (ex) {
    TEST(29, "TypeError", ex.name);
}

END();
