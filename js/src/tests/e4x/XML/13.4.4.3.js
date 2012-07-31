// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.3 - XML appendChild()");

TEST(1, true, XML.prototype.hasOwnProperty("appendChild"));

// Add new employee to list
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
    <employee id="2"><name>Sue</name><age>30</age></employee>
</employees>;

newEmp = <employee id="2"><name>Sue</name><age>30</age></employee>;

emps.appendChild(newEmp);
TEST(2, correct, emps);

// Add a new child element to the end of Jim's employee element
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employees>
    <employee id="0"><name>Jim</name><age>25</age><hobby>snorkeling</hobby></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

emps.employee.(name == "Jim").appendChild(<hobby>snorkeling</hobby>);
TEST(3, correct, emps);   

END();
