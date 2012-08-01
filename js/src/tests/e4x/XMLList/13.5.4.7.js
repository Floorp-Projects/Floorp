// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.7 - XMLList contains()");

TEST(1, true, XMLList.prototype.hasOwnProperty("contains"));

emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST(2, true, emps.employee.contains(<employee id="0"><name>Jim</name><age>25</age></employee>));
TEST(3, true, emps.employee.contains(<employee id="1"><name>Joe</name><age>20</age></employee>));
TEST(4, false, emps.employee.contains(<employee><name>Joe</name><age>20</age></employee>));

END();
