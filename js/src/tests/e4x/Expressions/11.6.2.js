// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.6.2 - XMLList Assignment");

// Set the name of the only customer in the order to Fred Jones
order =
<order>
    <customer>
        <name>John Smith</name>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
</order>;
   
correct =
<order>
    <customer>
        <name>Fred Jones</name>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
</order>;

order.customer.name = "Fred Jones";
TEST(1, correct, order);

// Replace all the hobbies for the only customer in the order
order =
<order>
    <customer>
        <name>John Smith</name>
        <hobby>Biking</hobby>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
</order>;

correct =
<order>
    <customer>
        <name>John Smith</name>
        <hobby>shopping</hobby>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
</order>;

order.customer.hobby = "shopping"
TEST(2, correct, order);

// Attempt to set the sale date of the item.  Throw an exception if more than 1 item exists.
order =
<order>
    <customer>
        <name>John Smith</name>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <saledate>01-05-2002</saledate>
    </item>
</order>;

correct =
<order>
    <customer>
        <name>John Smith</name>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <saledate>05-07-2002</saledate>
    </item>
</order>;

order.item.saledate = "05-07-2002"
TEST(3, correct, order);

order =
<order>
    <customer>
        <name>John Smith</name>
        <hobby>Biking</hobby>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
</order>;

try {
    order.item.saledate = "05-07-2002";
    SHOULD_THROW(4);
} catch (ex) {
    TEST(4, "TypeError", ex.name);
}

// Replace all the employee's hobbies with their new favorite pastime
emps =
<employees>
    <employee id = "1">
        <name>John</name>
        <age>20</age>
        <hobby>skiing</hobby>
    </employee>
    <employee id = "2">
        <name>Sue</name>
        <age>30</age>
        <hobby>running</hobby>
    </employee>
    <employee id = "3">
        <name>Ted</name>
        <age>35</age>
        <hobby>Biking</hobby>
    </employee>
</employees>;

correct =
<employees>
    <employee id = "1">
        <name>John</name>
        <age>20</age>
        <hobby>skiing</hobby>
    </employee>
    <employee id = "2">
        <name>Sue</name>
        <age>30</age>
        <hobby>running</hobby>
    </employee>
    <employee id = "3">
        <name>Ted</name>
        <age>35</age>
        <hobby>working</hobby>
    </employee>
</employees>;

emps.employee.(@id == 3).hobby = "working";
TEST(5, correct, emps);

// Replace the first employee with George
emps =
<employees>
    <employee id = "1">
        <name>John</name>
        <age>20</age>
    </employee>
    <employee id = "2">
        <name>Sue</name>
        <age>30</age>
    </employee>
    <employee id = "3">
        <name>Ted</name>
        <age>35</age>
    </employee>
</employees>;

correct =
<employees>
    <employee id = "4">
        <name>George</name>
        <age>27</age>
    </employee>
    <employee id = "2">
        <name>Sue</name>
        <age>30</age>
    </employee>
    <employee id = "3">
        <name>Ted</name>
        <age>35</age>
    </employee>
</employees>;

emps.employee[0] = <employee id="4"><name>George</name><age>27</age></employee>;
TEST(6, emps, correct);

// Add a new employee to the end of the employee list
emps =
<employees>
    <employee id = "1">
        <name>John</name>
        <age>20</age>
    </employee>
    <employee id = "2">
        <name>Sue</name>
        <age>30</age>
    </employee>
    <employee id = "3">
        <name>Ted</name>
        <age>35</age>
    </employee>
</employees>;

correct =
<employees>
    <employee id = "1">
        <name>John</name>
        <age>20</age>
    </employee>
    <employee id = "2">
        <name>Sue</name>
        <age>30</age>
    </employee>
    <employee id = "3">
        <name>Ted</name>
        <age>35</age>
    </employee>
    <employee id="4">
        <name>Frank</name>
        <age>39</age>
    </employee>
</employees>;

emps.employee += <employee id="4"><name>Frank</name><age>39</age></employee>;
TEST(7, correct, emps);

// Add a new employee to the end of the employee list
emps =
<employees>
    <employee id = "1">
        <name>John</name>
        <age>20</age>
    </employee>
    <employee id = "2">
        <name>Sue</name>
        <age>30</age>
    </employee>
    <employee id = "3">
        <name>Ted</name>
        <age>35</age>
    </employee>
</employees>;

correct =
<employees>
    <employee id = "1">
        <name>John</name>
        <age>20</age>
    </employee>
    <employee id = "2">
        <name>Sue</name>
        <age>30</age>
    </employee>
    <employee id = "3">
        <name>Ted</name>
        <age>35</age>
    </employee>
    <employee id="4">
        <name>Frank</name>
        <age>39</age>
    </employee>
</employees>;

emps.employee[emps.employee.length()] = <employee id="4"><name>Frank</name><age>39</age></employee>;
TEST(7, correct, emps);

END();
