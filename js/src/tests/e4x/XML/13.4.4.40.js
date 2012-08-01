// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.40 - valueOf");

TEST(1, true, XML.prototype.hasOwnProperty("valueOf"));

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

TEST(2, x, x.valueOf());

// Make sure they're the same and not copies

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

y = x.valueOf();

x.bravo = "two";

TEST(3, x, y);

END();
