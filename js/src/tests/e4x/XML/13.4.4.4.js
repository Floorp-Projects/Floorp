// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.4 - XML attribute()");

TEST(1, true, XML.prototype.hasOwnProperty("attribute"));

// Get count of employees
emps =
<employees count="2">
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST_XML(2, 2, emps.attribute("count"));

// Get the id of the employee age 25
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST_XML(3, 0, emps.employee.(age == "25").attribute("id"));

// Get the id of the employee named Jim
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST_XML(4, 0, emps.employee.(name == "Jim").attribute("id"));

END();
