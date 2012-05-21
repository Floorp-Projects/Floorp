/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.3.2 - QName Constructor");

q = new QName("*");
TEST(1, "object", typeof(q));
TEST(2, "*", q.localName);
TEST(3, null, q.uri);
TEST(4, "*::*", q.toString());

// Default namespace
q = new QName("foobar");
TEST(5, "object", typeof(q));
TEST(6, "foobar", q.localName);
TEST(7, "", q.uri);
TEST(8, "foobar", q.toString());

q = new QName("http://foobar/", "foobar");
TEST(9, "object", typeof(q));
TEST(10, "foobar", q.localName);
TEST(11, "http://foobar/", q.uri);
TEST(12, "http://foobar/::foobar", q.toString());

p = new QName(q);
TEST(13, typeof(p), typeof(q));
TEST(14, q.localName, p.localName);
TEST(15, q.uri, p.uri);

n = new Namespace("http://foobar/");
q = new QName(n, "foobar");
TEST(16, "object", typeof(q));

q = new QName(null);
TEST(17, "object", typeof(q));
TEST(18, "null", q.localName);
TEST(19, "", q.uri);
TEST(20, "null", q.toString());

q = new QName(null, null);
TEST(21, "object", typeof(q));
TEST(22, "null", q.localName);
TEST(23, null, q.uri);
TEST(24, "*::null", q.toString());

END();
