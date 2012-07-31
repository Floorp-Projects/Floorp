// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("9.1.1.6 - XML [[HasProperty]]");

x = <alpha attr1="value1">one<bravo>two<charlie>three</charlie></bravo></alpha>;
TEST(1, true, "bravo" in x);
TEST(2, true, 0 in x);
TEST(3, true, "charlie" in x.bravo);
TEST(4, true, 0 in x.bravo);
TEST(5, false, 1 in x);
TEST(6, false, 1 in x.bravo);
TEST(7, false, 2 in x);
TEST(8, false, 2 in x.bravo);
TEST(9, false, "foobar" in x);
TEST(10, false, "foobar" in x.bravo);

END();
