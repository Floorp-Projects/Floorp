/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("10.4 - toXMLList");

var x;

// null
try {
    x = null;
    x.toString();
    SHOULD_THROW(1);
} catch (ex) {
    TEST(1, "TypeError", ex.name);
}

// number
x = new Number(123);
TEST(2, "123", new XMLList(x).toXMLString());

// String
x = new String("<alpha><bravo>one</bravo></alpha>");
TEST(3, <alpha><bravo>one</bravo></alpha>, new XMLList(x));

x = new String("<alpha>one</alpha><charlie>two</charlie>");
TEST(4, "<alpha>one</alpha>\n<charlie>two</charlie>",
  new XMLList(x).toXMLString());

// XML
x = new XML(<alpha><bravo>one</bravo></alpha>);
TEST(5, <alpha><bravo>one</bravo></alpha>, new XMLList(x));

x = new XML(<root><alpha><bravo>one</bravo></alpha><charlie>two</charlie></root>);
TEST(6, <alpha><bravo>one</bravo></alpha>, new XMLList(x.alpha));

// XMLList
x = new XMLList(<alpha><bravo>one</bravo></alpha>);
TEST(7, <alpha><bravo>one</bravo></alpha>, new XMLList(x));

x = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(8, "<alpha>one</alpha>\n<bravo>two</bravo>",
  new XMLList(x).toXMLString());
   
END();
