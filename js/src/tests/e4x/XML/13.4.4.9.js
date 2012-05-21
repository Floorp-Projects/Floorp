/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.9 - XML comments()");

TEST(1, true, XML.prototype.hasOwnProperty("comments"));

XML.ignoreComments = false;

x =
<alpha>
    <!-- comment one -->
    <bravo>
    <!-- comment two -->
    some text
    </bravo>
</alpha>;   

TEST_XML(2, "<!-- comment one -->", x.comments());
TEST_XML(3, "<!-- comment two -->", x..*.comments());

END();
