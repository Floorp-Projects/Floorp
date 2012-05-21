/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("9.1.1.3 - XML [[Delete]]");

// .@
x =
<alpha attr1="value1">one</alpha>;

delete x.@attr1;
TEST_XML(1, "", x.@attr1);
TEST(2, <alpha>one</alpha>, x);

// ..@
x =
<alpha attr1="value1">
    one
    <bravo attr2="value2">
        two
        <charlie attr3="value3">three</charlie>
    </bravo>
</alpha>;

correct =
<alpha attr1="value1">
    one
    <bravo attr2="value2">
        two
        <charlie>three</charlie>
    </bravo>
</alpha>;

delete x..@attr3;
TEST(3, correct, x);

// ..@*
x =
<alpha attr1="value1">
    one
    <bravo attr2="value2">
        two
        <charlie attr3="value3">three</charlie>
    </bravo>
</alpha>;

correct =
<alpha>
    one
    <bravo>
        two
        <charlie>three</charlie>
    </bravo>
</alpha>;

delete x..@*;
TEST(4, correct, x);

x =
<BODY>
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY></BODY>;

delete x..HR;
TEST(5, correct, x);

x =
<BODY>
    ECMA-357
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY>
    ECMA-357
</BODY>;

delete x..HR;
TEST(6, correct.toXMLString(), x.toXMLString());

x =
<BODY>
    ECMA-357
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY>ECMA-357</BODY>;

delete x.HR;
TEST(7, correct, x);

x =
<BODY>
    ECMA-357
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY></BODY>;

delete x..*;
TEST(8, correct, x);

x =
<BODY>
    ECMA-357
    <HR id="i1"/>
    <HR id="i2"/>
    <HR id="i3"/>
    <HR id="i4"/>
</BODY>;

correct =
<BODY></BODY>;

delete x.*;
TEST(9, correct, x);

x =
<BODY>
    <UL>
      <LI id="i1"/>
      <LI id="i2"/>
      <LI id="i3"/>
    </UL>
</BODY>;
     
correct =
<BODY>
    <UL>
      <LI id="i1"/>
      <LI id="i2"/>
      <LI id="i3"/>
    </UL>
</BODY>;

delete x.LI;
TEST(10, correct, x);

x =
<BODY>
    <UL>
      <LI id="i1"/>
      <LI id="i2"/>
      <LI id="i3"/>
    </UL>
</BODY>;
     
correct =
<BODY>
    <UL></UL>
</BODY>;

delete x..LI;
TEST(11, correct, x);

END();
