// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.5.1 - Equality Operators");

x = <alpha>one</alpha>;
y = <alpha>one</alpha>;
TEST(1, true, (x == y) && (y == x));

// Should return false if comparison is not XML
y = "<alpha>one</alpha>";
TEST(2, false, (x == y) || (y == x));

y = undefined
TEST(3, false, (x == y) || (y == x));

y = null
TEST(4, false, (x == y) || (y == x));

// Should check logical equiv.
x = <alpha attr1="value1">one<bravo attr2="value2">two</bravo></alpha>;
y = <alpha attr1="value1">one<bravo attr2="value2">two</bravo></alpha>;
TEST(5, true, (x == y) && (y == x));

y = <alpha attr1="new value">one<bravo attr2="value2">two</bravo></alpha>;
TEST(6, false, (x == y) || (y == x));

m = new Namespace();
n = new Namespace();
TEST(7, true, m == n);

m = new Namespace("uri");
TEST(8, false, m == n);

n = new Namespace("ns", "uri");
TEST(9, true, m == n);

m = new Namespace(n);
TEST(10, true, m == n);

TEST(11, false, m == null);
TEST(12, false, null == m);

m = new Namespace("ns", "http://anotheruri");
TEST(13, false, m == n);

p = new QName("a");
q = new QName("b");
TEST(14, false, p == q);

q = new QName("a");
TEST(15, true, p == q);

q = new QName("http://someuri", "a");
TEST(16, false, p == q);

q = new QName(null, "a");
TEST(16, false, p == q);

END();
