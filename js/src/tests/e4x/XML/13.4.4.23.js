// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.23 - XML namespace()");

TEST(1, true, XML.prototype.hasOwnProperty("namespace"));

// Prefix case
x =
<foo:alpha xmlns:foo="http://foo/" xmlns:bar="http://bar/">
    <foo:bravo>one</foo:bravo>
</foo:alpha>;

y = x.namespace();
TEST(2, "object", typeof(y));
TEST(3, Namespace("http://foo/"), y);

y = x.namespace("bar");
TEST(4, "object", typeof(y));
TEST(5, Namespace("http://bar/"), y);

// No Prefix Case
x =
<alpha xmlns="http://foobar/">
    <bravo>one</bravo>
</alpha>;

y = x.namespace();
TEST(6, "object", typeof(y));
TEST(7, Namespace("http://foobar/"), y);

// No Namespace case
x =
<alpha>
    <bravo>one</bravo>
</alpha>;
TEST(8, Namespace(""), x.namespace());

// Namespaces of attributes
x =
<alpha xmlns="http://foo/">
    <ns:bravo xmlns:ns="http://bar" name="two" ns:value="three">one</ns:bravo>
</alpha>;

var ns = new Namespace("http://bar");
y = x.ns::bravo.@name.namespace();
TEST(9, Namespace(""), y);

y = x.ns::bravo.@ns::value.namespace();
TEST(10, ns.toString(), y.toString());

END();
