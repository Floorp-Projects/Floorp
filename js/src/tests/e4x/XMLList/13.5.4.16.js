/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.16 - XMLList parent()");

TEST(1, true, XMLList.prototype.hasOwnProperty("parent"));
   
// Empty should return undefined
x = new XMLList();
TEST(2, undefined, x.parent());

// If all XMLList items have same parent, then return that parent.
x =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
    <bravo>three<charlie>four</charlie></bravo>
</alpha>;

y = new XMLList();
y += x.bravo;
y += x.charlie;

TEST(3, x, y.parent());

// If items have different parents then return undefined
y = new XMLList();
y += x.bravo;
y += x.bravo.charlie;

TEST(4, undefined, y.parent());

y = x..charlie;

TEST(5, undefined, y.parent());

END();
