/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.14 - XMLList length()");

TEST(1, true, XMLList.prototype.hasOwnProperty("length"));

x = <><alpha>one</alpha></>;

TEST(2, 1, x.length());

x = <><alpha>one</alpha><bravo>two</bravo></>;

TEST(2, 2, x.length());

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

TEST(3,2,emps..name.length());

END();
