/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.25 - XML nodeKind()");

TEST(1, true, XML.prototype.hasOwnProperty("nodeKind"));

x =
<alpha attr1="value1">
    <bravo>one</bravo>
</alpha>;

TEST(2, "element", x.bravo.nodeKind());
TEST(3, "attribute", x.@attr1.nodeKind());

// Nonexistent node type is text
x = new XML();
TEST(4, "text", x.nodeKind());
TEST(5, "text", XML.prototype.nodeKind());

END();
