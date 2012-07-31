// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.2.2 - Function Calls");


rectangle = <rectangle>
            <x>50</x>
            <y>75</y>
            <length>20</length>
            <width>30</width>
            </rectangle>;   


TEST(1, 1, rectangle.length());

TEST(2, <length>20</length>, rectangle.length);

shipto = <shipto>
         <name>Fred Jones</name>
         <street>123 Foobar Ave.</street>
         <citystatezip>Redmond, WA, 98008</citystatezip>
         </shipto>;


upperName = shipto.name.toUpperCase();
TEST(3, "FRED JONES", upperName);

upperName = shipto.name.toString().toUpperCase();
TEST(4, "FRED JONES", upperName);
upperName = shipto.name.toUpperCase();
TEST(5, "FRED JONES", upperName);

citystatezip = shipto.citystatezip.split(", ");
state = citystatezip[1];
TEST(6, "WA", state);
zip = citystatezip[2];
TEST(7, "98008", zip);

citystatezip = shipto.citystatezip.toString().split(", ");
state = citystatezip[1];
TEST(8, "WA", state);
zip = citystatezip[2];
TEST(9, "98008", zip);

// Test method name/element name conflicts

x =
<alpha>
    <name>Foo</name>
    <length>Bar</length>
</alpha>;

TEST(10, <name>Foo</name>, x.name);
TEST(11, QName("alpha"), x.name());
TEST(12, <length>Bar</length>, x.length);
TEST(13, 1, x.length());
TEST(14, x, x.(name == "Foo"));
x.name = "foobar";
TEST(15, <name>foobar</name>, x.name);
TEST(16, QName("alpha"), x.name());



END();
