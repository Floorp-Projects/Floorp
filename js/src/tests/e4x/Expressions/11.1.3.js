// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.1.3 - Wildcard Identifiers");

x =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>

correct = <><bravo>one</bravo><charlie>two</charlie></>;
TEST(1, correct, x.*);

END();
