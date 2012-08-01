// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START('9.1.1.2 - XML [[Put]]');


// .
var x =
<alpha attr1="value1" attr2="value2">
    <bravo>
        one
        <charlie>two</charlie>
    </bravo>
</alpha>;

var correct =
<charlie>new</charlie>;

x.bravo.charlie = "new"
TEST(1, correct, x.bravo.charlie)
x.bravo = <delta>three</delta>
TEST(2, "three", x.delta.toString())

// .@
x = <alpha attr1="value1" attr2="value2"><bravo>one<charlie>two</charlie></bravo></alpha>
x.@attr1 = "newValue"
TEST_XML(3, "newValue", x.@attr1)

// From Martin

XML.prettyPrinting = false;

var god = <god><name>Kibo</name></god>;
god.name = <><prename>James</prename><nickname>Kibo</nickname></>;

var expect = '<god><prename>James</prename><nickname>Kibo</nickname></god>';
actual = god;
TEST_XML(4, expect, actual);

END();
