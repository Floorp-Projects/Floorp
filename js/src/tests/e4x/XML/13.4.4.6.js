/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.6 - XML child()");

TEST(1, true, XML.prototype.hasOwnProperty("child"));

emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employee id="0"><name>Jim</name><age>25</age></employee> +
<employee id="1"><name>Joe</name><age>20</age></employee>;


TEST(2, correct, emps.child("employee"));

TEST(3, <name>Joe</name>, emps.employee[1].child("name"));

correct = <employee id="1"><name>Joe</name><age>20</age></employee>;

TEST(4, correct, emps.child(1));

END();
