/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.11 - XML copy()");

TEST(1, true, XML.prototype.hasOwnProperty("copy"));

emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employee id="0"><name>Jim</name><age>25</age></employee>;

x = emps.employee[0].copy();

TEST(2, null, x.parent());
TEST(3, correct, x);

// Make sure we're getting a copy, not a ref to orig.
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employee id="0"><name>Jim</name><age>25</age></employee>

empCopy = emps.employee[0].copy();

emps.employee[0].name[0] = "Sally";

TEST(4, correct, empCopy);

// Try copying whole XML twice
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

empCopy = emps.copy();
x = empCopy.copy();

TEST(5, x, emps);

END();
