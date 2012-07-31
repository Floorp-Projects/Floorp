// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.4 - XMLList child()");

TEST(1, true, XMLList.prototype.hasOwnProperty("child"));

// Test with XMLList of size 0
x = new XMLList()
TEST(2, "xml", typeof(x.child("bravo")));
TEST_XML(3, "", x.child("bravo"));

// Test with XMLList of size 1
x += <alpha>one<bravo>two</bravo></alpha>;   
TEST(4, "xml", typeof(x.child("bravo")));
TEST_XML(5, "<bravo>two</bravo>", x.child("bravo"));

x += <charlie><bravo>three</bravo></charlie>;
TEST(6, "xml", typeof(x.child("bravo")));

correct = <><bravo>two</bravo><bravo>three</bravo></>;
TEST(7, correct, x.child("bravo"));

// Test no match, null and undefined
TEST(8, "xml", typeof(x.child("foobar")));
TEST_XML(9, "", x.child("foobar"));

try {
  x.child(null);
  SHOULD_THROW(10);
} catch (ex) {
  TEST(10, "TypeError", ex.name);
}

// Test numeric inputs
x =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

TEST(12, <bravo>one</bravo>, x.child(0));
TEST(13, <charlie>two</charlie>, x.child(1));

END();
