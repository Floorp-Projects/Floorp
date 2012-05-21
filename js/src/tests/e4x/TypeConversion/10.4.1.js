/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("10.4.1 - toXMLList Applied to String type");

var x, y, correct;

x =
<>
    <alpha>one</alpha>
    <bravo>two</bravo>
</>;

TEST(1, "xml", typeof(x));
TEST(2, "<alpha>one</alpha>\n<bravo>two</bravo>", x.toXMLString());

// Load from another XMLList
// Make sure it is copied if it's an XMLList
y = new XMLList(x);

x += <charlie>three</charlie>;

TEST(3, "<alpha>one</alpha>\n<bravo>two</bravo>", y.toXMLString());
  
// Load from one XML type
x = new XMLList(<alpha>one</alpha>);
TEST(4, "<alpha>one</alpha>", x.toXMLString());

// Load from Anonymous
x = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(5, "<alpha>one</alpha>\n<bravo>two</bravo>", x.toXMLString());

// Load from Anonymous as string
x = new XMLList("<alpha>one</alpha><bravo>two</bravo>");
TEST(6, "<alpha>one</alpha>\n<bravo>two</bravo>", x.toXMLString());

// Load from illegal type
//x = new XMLList("foobar");
//ADD(7, "", x);

John = "<employee><name>John</name><age>25</age></employee>";
Sue = "<employee><name>Sue</name><age>32</age></employee>";

correct =
<>
    <employee><name>John</name><age>25</age></employee>
    <employee><name>Sue</name><age>32</age></employee>
</>;

var1 = new XMLList(John + Sue);

TEST(8, correct, var1);

END();
