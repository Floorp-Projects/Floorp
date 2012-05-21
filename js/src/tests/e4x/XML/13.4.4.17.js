/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.17 - XML inScopeNamespaces()");

TEST(1, true, XML.prototype.hasOwnProperty("inScopeNamespaces"));

x =
<alpha xmlns:foo="http://foo/" xmlns:bar="http://bar/">
    <bravo>one</bravo>
</alpha>;

namespaces = x.bravo.inScopeNamespaces();

TEST(2, "foo", namespaces[0].prefix);
TEST(3, "http://foo/", namespaces[0].uri);
TEST(4, "bar", namespaces[1].prefix);
TEST(5, "http://bar/", namespaces[1].uri);
TEST(6, "", namespaces[2].prefix);
TEST(7, "", namespaces[2].uri);
TEST(8, 3, namespaces.length);

END();
