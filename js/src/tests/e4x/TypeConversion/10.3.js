// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("10.3 - toXML");

var x;

// boolean
x = new Boolean(true);
TEST_XML(1, "true", new XML(x));

// number
x = new Number(123);
TEST_XML(2, "123", new XML(x));

// String
x = new String("<alpha><bravo>one</bravo></alpha>");
TEST(3, <alpha><bravo>one</bravo></alpha>, new XML(x));

// XML
x = new XML(<alpha><bravo>one</bravo></alpha>);
TEST(4, <alpha><bravo>one</bravo></alpha>, new XML(x));

// XMLList
x = new XMLList(<alpha><bravo>one</bravo></alpha>);
TEST(5, <alpha><bravo>one</bravo></alpha>, new XML(x));

try {
    x = new XMLList(<alpha>one</alpha> + <bravo>two</bravo>);
    new XML(x);
    SHOULD_THROW(6);
} catch (ex) {
    TEST(6, "TypeError", ex.name);
}

END();
