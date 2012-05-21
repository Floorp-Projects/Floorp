/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.7 - XML childIndex()");

TEST(1, true, XML.prototype.hasOwnProperty("childIndex"));

emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST(2, 0, emps.employee[0].childIndex());

// Get the ordinal index of the employee named Joe
TEST(3, 1, emps.employee.(age == "20").childIndex());

TEST(4, 1, emps.employee.(name == "Joe").childIndex());
   
END();
