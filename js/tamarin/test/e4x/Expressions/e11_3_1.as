/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *   Ethan Hugg
 *   Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
correct =
<order id="123456">
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


// delete all description tags with descendant operator
// is not supposed to do anything, see bug 149397
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
   

delete order..description;
TEST(10, correct, order);

order = 
<order id="123456">
    <customer id="123">
        <firstname>John</firstname>
        <lastname>Doe</lastname>
        <address>123 Foobar Ave.</address>
        <city>Bellevue</city>
        <state>WA</state>
    </customer>
    <customer id="456">
            <firstname>Mary</firstname>
            <lastname>Jones</lastname>
            <address>456 Foobar Ave.</address>
            <city>Bel Air</city>
            <state>CA</state>
    </customer>
</order>; 

try {
	delete order.customer.(firstname == "John");
	result = order;
} catch (e1) {
	result = typeError(e1.toString());
}

AddTestCase("Delete an XMLList", "TypeError: Error #1119", result);

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

// delete all id attributes
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
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item>
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
</order>;    
   

delete order.item.@id;
TEST(11, correct, order);

order = 
<order id="123456">
  <blah>
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
  </blah>
</order>;

// delete all id attributes, using descendant operator
correct = 
<order id="123456">
  <blah>
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
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
    <item>
        <description>DVD Player</description>
        <price>399.99</price>
        <quantity>1</quantity>
    </item>
  </blah>
</order>;    
   

delete order..item.@id;
TEST(12, correct, order);

//default xml namespace = "http://someuri";
x1 = <x/>;
x1.a.b = "foo";
delete x1.a.b;
TEST_XML(10, "", x1.a.b);

var ns = new Namespace("");
x1.a.b = <b xmlns="">foo</b>;
TEST(11, "foo", x1.a.ns::b.toString());

delete x1.a.b;
TEST(12, "", x1.a.ns::b.toString());

delete x1.a.ns::b;
TEST_XML(13, "", x1.a.ns::b);

var y1;
x1 = new XML("<a><b><c>C</c><d>D</d></b></a>");
y1 = new XML("<a><b><c>C</c></b></a>");
 
AddTestCase("delete XML:", true, (delete x1.b.d, (x1 == y1)));


 
x1 = new XMLList("<d><a>A</a><b>B</b><c>C</c></d>");
y1 = new XMLList("<d><a>A</a><c>C</c></d>");

AddTestCase("delete XMLList:", true, (delete x1.b, (x1 == y1)));

END();
