// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.21 - XML localName()");

TEST(1, true, XML.prototype.hasOwnProperty("localName"));

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

y = x.bravo.localName();
TEST(2, "string", typeof(y));
TEST(3, "bravo", y);

y = x.bravo.localName();
x.bravo.setNamespace("http://someuri");
TEST(4, "bravo", y);

x =
<foo:alpha xmlns:foo="http://foo/">
    <foo:bravo name="bar" foo:value="one">one</foo:bravo>
</foo:alpha>;

ns = new Namespace("http://foo/");
y = x.ns::bravo.localName();
TEST(5, "string", typeof(y));
TEST(6, "bravo", y);

y = x.ns::bravo.@name.localName();
TEST(7, "name", y);

y = x.ns::bravo.@ns::value.localName();
TEST(8, "value", y);

END();
