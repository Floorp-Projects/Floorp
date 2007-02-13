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

//order.customer.name = "Fred Jones";
//TEST(1, correct, order);

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

//emps.employee.(@id == 3).hobby = "working";
//TEST(5, correct, emps);

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


// property name is NOT array index

var x1 = new XML("<a><b><c>A</c><d>B</d></b></a>");

x1.b.d = "C";

var y1 = new XML("<a><b><c>A</c><d>C</d></b></a>");

AddTestCase( "property name is not array index :", true, (x1==y1) );


x1 = new XML("<a><b><c>A</c><d>B</d></b></a>");

x1.v.b.d = "C";

y1 = new XML("<a><b><c>A</c><d>B</d></b><v><b><d>C</d></b></v></a>");

AddTestCase("Adding new nested node at root: " , true, (x1 == y1));


// property name is array index

x1 = new XML("<a><b><c>A0</c><d>B0</d></b><b><c>A1</c><d>B1</d></b><b><c>A2</c><d>B2</d></b></a>");

x1.b[1] = new XML("<b><c>A3</c><d>B3</d></b>");

y1 = new XML("<a><b><c>A0</c><d>B0</d></b><b><c>A3</c><d>B3</d></b><b><c>A2</c><d>B2</d></b></a>");

AddTestCase( "property name exists in XMLList     :", true, (x1==y1) );


// property name does NOT exist in XMLList

x1.b[3] = new XML("<b><c>A4</c><d>B4</d></b>");

y1 = new XML("<a><b><c>A0</c><d>B0</d></b><b><c>A3</c><d>B3</d></b><b><c>A2</c><d>B2</d></b><b><c>A4</c><d>B4</d></b></a>");

AddTestCase( "property name does NOT exist in XMLList :", true, (x1==y1) );


// property is XML with non-null parent

x1 = new XML("<a><b><c>A0</c><d>B0</d></b><b><c>A1</c><d>B1</d></b><b><c>A2</c><d>B2</d></b></a>");

x1.b = new XMLList("<b>A</b><c>B</c>");

y1 = new XML("<a><b>A</b><c>B</c></a>");

AddTestCase( "property is XML with non-null parent    :", true, (x1==y1) );


// AssignmentExpression = XML value

x1 = new XML("<a><b><c>A0</c><d>B0</d></b><b><c>A1</c><d>B1</d></b><b><c>A2</c><d>B2</d></b></a>");

x1.b[0] = new XML("<b><c>A3</c><d>B3</d></b>");

y1 = new XML("<a><b><c>A3</c><d>B3</d></b><b><c>A1</c><d>B1</d></b><b><c>A2</c><d>B2</d></b></a>");

AddTestCase( "AssignmentExpression = XML value        :", true, (x1==y1) );


// AssignmentExpression = XMLList object

x1 = new XML("<a><b><c>A0</c><d>B0</d></b><b><c>A1</c><d>B1</d></b><b><c>A2</c><d>B2</d></b></a>");

x1.b = new XMLList("<b>A</b><b>B</b><b>C</b>");

y1 = new XML("<a><b>A</b><b>B</b><b>C</b></a>");

AddTestCase( "AssignmentExpression = XMLList object   :", true, (x1==y1) );


// AssignmentExpression != XML/XMLList

x1 = new XML("<a><b><c>A0</c><d>B0</d></b><b><c>A1</c><d>B1</d></b><b><c>A2</c><d>B2</d></b></a>");

x1.b = "Hello World";

y1 = new XML("<a><b>Hello World</b></a>");

AddTestCase( "AssignmentExpression != XML/XMLList     :", true, (x1==y1) );

END();
