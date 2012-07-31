// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.3.1 - Delete Operator");

order =
<order id="123456">
    <customer id="123">
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <address>123 Foobar Ave.</address>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="3456">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="56789">
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
</order>;   

// Delete the customer address
correct =
<order id="123456">
    <customer id="123">
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="3456">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="56789">
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
</order>;   

delete order.customer.address;
TEST_XML(1, "", order.customer.address);
TEST(2, correct, order);

order =
<order id="123456">
    <customer id="123">
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <address>123 Foobar Ave.</address>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="3456">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="56789">
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
</order>;   

// delete the custmomer ID
correct =
<order id="123456">
    <customer>
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <address>123 Foobar Ave.</address>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="3456">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="56789">
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
</order>;   

delete order.customer.@id;
TEST_XML(3, "", order.customer.@id);
TEST(4, correct, order);

order =
<order id="123456">
    <customer id="123">
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <address>123 Foobar Ave.</address>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="3456">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="56789">
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
</order>;   

// delete the first item price
correct =
<order id="123456">
    <customer id="123">
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <address>123 Foobar Ave.</address>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <quantity>1</quantity>
    </item>
    <item id="3456">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="56789">
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
</order>;   

delete order.item.price[0];
TEST_XML(5, "", order.item[0].price);
TEST(6, <price>1299.99</price>, order.item.price[0]);
TEST(7, order, correct);

order =
<order id="123456">
    <customer id="123">
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <address>123 Foobar Ave.</address>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="3456">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item id="56789">
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
</order>;   

// delete all the items
correct =<order id="123456">
    <customer id="123">
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <address>123 Foobar Ave.</address>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
</order>;   

delete order.item;
TEST_XML(8, "", order.item);
TEST(9, correct, order);

default xml namespace = "http://someuri";
x = <x/>;
x.a.b = "foo";
delete x.a.b;
TEST_XML(10, "", x.a.b);

var ns = new Namespace("");
x.a.b = <b xmlns="">foo</b>;
delete x.a.b;
TEST(11, "foo", x.a.ns::b.toString());

delete x.a.ns::b;
TEST_XML(12, "", x.a.ns::b);

END();
