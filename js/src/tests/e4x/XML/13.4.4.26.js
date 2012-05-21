/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.26 - XML normalize()");

TEST(1, true, XML.prototype.hasOwnProperty("normalize"));

XML.ignoreWhitespace = false;
XML.prettyPrinting = false;

var x = <alpha> <bravo> one </bravo> </alpha>;

TEST_XML(2, "<alpha> <bravo> one </bravo> </alpha>", x);
x.normalize();
TEST_XML(3, "<alpha> <bravo> one </bravo> </alpha>", x);


// First, test text node coalescing

delete x.bravo[0];

TEST_XML(4, "<alpha>  </alpha>", x);
TEST(5, 2, x.children().length());

x.normalize();

TEST_XML(6, "<alpha>  </alpha>", x);
TEST(7, 1, x.children().length());

// check that nodes are inserted in the right place after a normalization

x.appendChild(<bravo> fun </bravo>);

TEST_XML(8, "<alpha>  <bravo> fun </bravo></alpha>", x);
TEST(9, 2, x.children().length());

// recursive nature

var y = <charlie> <delta/> </charlie>;

TEST(10, 3, y.children().length());

x.appendChild(y);
delete y.delta[0];

TEST(11, 2, y.children().length());

x.normalize();

TEST(12, 1, y.children().length());
TEST(13, 1, x.charlie.children().length());



// Second, test empty text node removal

x = <alpha><beta/></alpha>;

TEST_XML(14, "<alpha><beta/></alpha>", x);
TEST(15, 1, x.children().length());

x.appendChild(XML());

TEST_XML(16, "<alpha><beta/></alpha>", x);
TEST(17, 2, x.children().length());

x.normalize();

TEST_XML(18, "<alpha><beta/></alpha>", x);
TEST(19, 1, x.children().length());

x.appendChild(XML(" "));

TEST_XML(20, "<alpha><beta/> </alpha>", x);
TEST(21, 2, x.children().length());

x.normalize();

// normalize does not remove whitespace-only text nodes
TEST_XML(22, "<alpha><beta/> </alpha>", x);
TEST(23, 2, x.children().length());

y = <foo/>;
y.appendChild(XML());

TEST(24, 1, y.children().length());

x.appendChild(y);

// check recursive nature

x.normalize();

TEST(25, 0, y.children().length());

END();
