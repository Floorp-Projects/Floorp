// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("9.2.1.8 XMLList [[Descendants]]");

x =
<>
<alpha><bravo>one</bravo></alpha>
<charlie><delta><bravo>two</bravo></delta></charlie>
</>;

correct = <><bravo>one</bravo><bravo>two</bravo></>;
TEST(1, correct, x..bravo);

END();
