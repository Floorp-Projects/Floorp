/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.29 - XML prependChild()");

TEST(1, true, XML.prototype.hasOwnProperty("prependChild"));

x =
<alpha>
    <bravo>
        <charlie>one</charlie>
    </bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        <foo>bar</foo>
        <charlie>one</charlie>
    </bravo>
</alpha>;

x.bravo.prependChild(<foo>bar</foo>);

TEST(2, correct, x);

emps =
<employees>
    <employee>
        <name>John</name>
    </employee>
    <employee>
        <name>Sue</name>
    </employee>
</employees>   

correct =
<employees>
    <employee>
        <prefix>Mr.</prefix>
        <name>John</name>
    </employee>
    <employee>
        <name>Sue</name>
    </employee>
</employees>   

emps.employee.(name == "John").prependChild(<prefix>Mr.</prefix>);

TEST(3, correct, emps);

END();
