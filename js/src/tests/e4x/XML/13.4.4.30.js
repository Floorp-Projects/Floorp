/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.30 - propertyIsEnumerable()");

TEST(1, true, XML.prototype.hasOwnProperty("propertyIsEnumerable"));

// All properties accessible by Get are enumerable
x =
<alpha>
    <bravo>one</bravo>
</alpha>;

TEST(2, true, x.propertyIsEnumerable("0"));
TEST(3, true, x.propertyIsEnumerable(0));
TEST(5, false, x.propertyIsEnumerable("bravo"));
TEST(6, false, x.propertyIsEnumerable());
TEST(7, false, x.propertyIsEnumerable(undefined));
TEST(8, false, x.propertyIsEnumerable(null));

END();
