/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.14 - XML hasOwnProperty()");

TEST(1, true, XML.prototype.hasOwnProperty("hasOwnProperty"));
   
x =
<alpha attr1="value1">
    <bravo>one</bravo>
    <charlie>
        two
        three
        <echo>four</echo>
    </charlie>
    <delta />
</alpha>;

// Returns true for elements/attributes
TEST(2, true, x.hasOwnProperty("bravo"));
TEST(3, true, x.hasOwnProperty("@attr1"));
TEST(4, false, x.hasOwnProperty("foobar"));

// Test for XML Prototype Object - returns true for XML methods.
TEST(5, true, XML.prototype.hasOwnProperty("toString"));
TEST(6, false, XML.prototype.hasOwnProperty("foobar"));

END();
