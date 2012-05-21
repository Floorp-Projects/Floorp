/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("10.3.1 - toXML applied to String type");

john = "<employee><name>John</name><age>25</age></employee>";
sue = "<employee><name>Sue</name><age>32</age></employee>";
tagName = "employees";
employees = new XML("<" + tagName + ">" + john + sue + "</" + tagName + ">");

correct =
<employees>
    <employee><name>John</name><age>25</age></employee>
    <employee><name>Sue</name><age>32</age></employee>
</employees>;

TEST(1, correct, employees);

END();
