// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.2 - XMLList Constructor");

x = new XMLList();
TEST(1, "xml", typeof(x));  
TEST(2, true, x instanceof XMLList);

// Load from another XMLList
// Make sure it is copied if it's an XMLList
x =
<>
    <alpha>one</alpha>
    <bravo>two</bravo>
</>;

y = new XMLList(x);

x += <charlie>three</charlie>;

TEST(3, "<alpha>one</alpha>\n<bravo>two</bravo>", y.toString());
  
// Load from one XML type
x = new XMLList(<alpha>one</alpha>);
TEST_XML(4, "<alpha>one</alpha>", x);

// Load from Anonymous
x = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(5, "<alpha>one</alpha>\n<bravo>two</bravo>", x.toString());

// Load from Anonymous as string
x = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(6, "<alpha>one</alpha>\n<bravo>two</bravo>", x.toString());

// Load from single textnode
x = new XMLList("foobar");
TEST_XML(7, "foobar", x);

x = XMLList(7);
TEST_XML(8, 7, x);

// Undefined and null should behave like ""
x = new XMLList(null);
TEST_XML(9, "", x);

x = new XMLList(undefined);
TEST_XML(10, "", x);

END();
