/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.6.3 - Compound Assignment");

// Insert employee 3 and 4 after the first employee
e =
<employees>
    <employee id="1">
        <name>Joe</name>
        <age>20</age>
    </employee>
    <employee id="2">
        <name>Sue</name>
        <age>30</age>
    </employee>
</employees>;

correct =
<employees>
    <employee id="1">
        <name>Joe</name>
        <age>20</age>
    </employee>
    <employee id="3">
        <name>Fred</name>
    </employee>
    <employee id="4">
        <name>Carol</name>
    </employee>
    <employee id="2">
        <name>Sue</name>
        <age>30</age>
    </employee>
</employees>;
   
e.employee[0] += <employee id="3"><name>Fred</name></employee> +
    <employee id="4"><name>Carol</name></employee>;
   
TEST(1, correct, e);

// Append employees 3 and 4 to the end of the employee list
e =
<employees>
    <employee id="1">
        <name>Joe</name>
        <age>20</age>
    </employee>
    <employee id="2">
        <name>Sue</name>
        <age>30</age>
    </employee>
</employees>;

correct =
<employees>
    <employee id="1">
        <name>Joe</name>
        <age>20</age>
    </employee>
    <employee id="2">
        <name>Sue</name>
        <age>30</age>
    </employee>
    <employee id="3">
        <name>Fred</name>
    </employee>
    <employee id="4">
        <name>Carol</name>
    </employee>
</employees>;

e.employee[1] += <employee id="3"><name>Fred</name></employee> +
    <employee id="4"><name>Carol</name></employee>;
TEST(2, correct, e);
       

END();
