// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.1 - XMLList Constructor as Function");

x = XMLList();

TEST(1, "xml", typeof(x));  
TEST(2, true, x instanceof XMLList);

// Make sure it's not copied if it's an XMLList
x =
<>
    <alpha>one</alpha>
    <bravo>two</bravo>
</>;

y = XMLList(x);
TEST(3, x === y, true);

x += <charlie>three</charlie>;
TEST(4, x === y, false);

// Load from one XML type
x = XMLList(<alpha>one</alpha>);
TEST_XML(5, "<alpha>one</alpha>", x);

// Load from Anonymous
x = XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
correct = new XMLList();
correct += <alpha>one</alpha>;
correct += <bravo>two</bravo>;
TEST_XML(6, correct.toString(), x);

// Load from Anonymous as string
x = XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
correct = new XMLList();
correct += <alpha>one</alpha>;
correct += <bravo>two</bravo>;
TEST_XML(7, correct.toString(), x);

// Load from single textnode
x = XMLList("foobar");
TEST_XML(8, "foobar", x);

x = XMLList(7);
TEST_XML(9, "7", x);

// Undefined and null should behave like ""
x = XMLList(null);
TEST_XML(10, "", x);

x = XMLList(undefined);
TEST_XML(11, "", x);

END();
