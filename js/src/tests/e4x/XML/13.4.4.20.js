// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.20 - XML length()");

TEST(1, true, XML.prototype.hasOwnProperty("length"));

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

TEST(2, 1, x.length());
TEST(3, 1, x.bravo.length());
TEST(4, 1, x.charlie.length());
TEST(5, 1, x.delta.length());

END();
