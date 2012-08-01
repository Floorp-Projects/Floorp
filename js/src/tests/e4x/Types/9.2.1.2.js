// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("9.2.1.2 - XMLList [[Put]]");

var x =
<>
    <alpha>one</alpha>
    <bravo>two</bravo>
</>;

TEST(1, "<alpha>one</alpha>\n<bravo>two</bravo>",
  x.toXMLString());

x[0] = <charlie>three</charlie>;
TEST(2, "<charlie>three</charlie>\n<bravo>two</bravo>",
  x.toXMLString());

x[0] = <delta>four</delta> + <echo>five</echo>;
TEST(3, "<delta>four</delta>\n<echo>five</echo>\n<bravo>two</bravo>",
  x.toXMLString());

END();
