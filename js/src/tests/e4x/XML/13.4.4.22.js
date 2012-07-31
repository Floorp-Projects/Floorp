// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.22 - XML name()");

TEST(1, true, XML.prototype.hasOwnProperty("name"));

x =
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

y = x.bravo.name();

TEST(2, "object", typeof(y));
TEST(3, QName("bravo"), y);

x =
<foo:alpha xmlns:foo="http://foo/">
    <foo:bravo name="one" foo:value="two">one</foo:bravo>
</foo:alpha>;

ns = new Namespace("http://foo/");
y = x.ns::bravo.name();

TEST(4, "object", typeof(y));
TEST(5, QName("http://foo/", "bravo"), y);

y = x.ns::bravo.@name.name();
TEST(6, QName("name"), y);

y = x.ns::bravo.@ns::value.name();
TEST(7, "http://foo/", y.uri);
TEST(8, "value", y.localName);
TEST(9, QName("http://foo/", "value"), y);

END();
