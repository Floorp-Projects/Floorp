/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 * Ethan Hugg
 * Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

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