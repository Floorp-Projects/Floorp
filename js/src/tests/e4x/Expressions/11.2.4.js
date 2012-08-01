// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.2.4 - XML Filtering Predicate Operator");

e = <employees>
    <employee id="0"><name>John</name><age>20</age></employee>
    <employee id="1"><name>Sue</name><age>30</age></employee>
    </employees>;


correct = <employee id="0"><name>John</name><age>20</age></employee>;

john = e.employee.(name == "John");
TEST(1, correct, john);   

john = e.employee.(name == "John");
TEST(2, correct, john);   

correct =
<employee id="0"><name>John</name><age>20</age></employee> +
<employee id="1"><name>Sue</name><age>30</age></employee>;

twoEmployees = e.employee.(@id == 0 || @id == 1);
TEST(3, correct, twoEmployees);

twoEmployees = e.employee.(@id == 0 || @id == 1);
TEST(4, correct, twoEmployees);

i = 0;
twoEmployees = new XMLList();
for each (var p in e..employee)
{
    if (p.@id == 0 || p.@id == 1)
    {
        twoEmployees += p;
    }
}
TEST(5, correct, twoEmployees);

i = 0;
twoEmployees = new XMLList();
for each (var p in e..employee)
{
    if (p.@id == 0 || p.@id == 1)
    {
        twoEmployees[i++] = p;
    }
}
TEST(6, correct, twoEmployees);

// test with syntax
e = <employees>
    <employee id="0"><name>John</name><age>20</age></employee>
    <employee id="1"><name>Sue</name><age>30</age></employee>
    </employees>;

correct =
<employee id="0"><name>John</name><age>20</age></employee> +
<employee id="1"><name>Sue</name><age>30</age></employee>;

i = 0;
twoEmployees = new XMLList();
for each (var p in e..employee)
{
    with (p)
    {
        if (@id == 0 || @id == 1)
        {
            twoEmployees[i++] = p;
        }
    }
}
TEST(7, correct, twoEmployees);

END();
