// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.2 - XML addNamespace()");

TEST(1, true, XML.prototype.hasOwnProperty("addNamespace"));

e =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

n = "http://foobar/";
e.addNamespace(n);

n = new Namespace();
e.addNamespace(n);

n = new Namespace("http://foobar/");
e.addNamespace(n);

x = <a/>;
n = new Namespace("ns", "uri");
x.addNamespace(n);
TEST(2, "<a xmlns:ns=\"uri\"/>", x.toXMLString());

END();
