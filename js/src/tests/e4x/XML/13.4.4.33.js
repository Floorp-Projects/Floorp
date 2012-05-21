/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.33 - XML setChildren()");

TEST(1, true, XML.prototype.hasOwnProperty("setChildren"));

x =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <charlie>two</charlie>
</alpha>;

x.setChildren(<charlie>two</charlie>);

TEST(2, correct, x);

// Replace the entire contents of Jim's employee element
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

correct =
<employees>
    <employee id="0"><name>John</name><age>35</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

emps.employee.(name == "Jim").setChildren(<name>John</name> + <age>35</age>);

TEST(3, correct, emps);

END();
