/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.32 - XML replace()");

TEST(1, true, XML.prototype.hasOwnProperty("replace"));

// Replace the first employee record with an open staff requisition
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employees>
    <requisition status="open" />
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

emps.replace(0, <requisition status="open" />);

TEST(2, correct, emps);

// Replace all children with open staff requisition

emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employees>
    <requisition status="open" />
</employees>;

emps.replace("*", <requisition status="open" />);

TEST(3, correct, emps);

// Replace all employee elements with open staff requisition

emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employees>
    <requisition status="open" />
</employees>;

emps.replace("employee", <requisition status="open" />);

TEST(4, correct, emps);

END();
