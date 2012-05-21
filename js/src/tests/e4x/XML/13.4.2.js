/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.2 - XML Constructor");

x = new XML();
TEST(1, "xml", typeof(x));
TEST(2, true, x instanceof XML);

correct =
<Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:stock="http://mycompany.com/stocks"
    soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <Body>
        <stock:GetLastTradePrice>
            <stock:symbol>DIS</stock:symbol>
        </stock:GetLastTradePrice>
    </Body>
</Envelope>;

x = new XML(correct);
TEST_XML(3, correct.toXMLString(), x);

text =
"<Envelope" +
"    xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\"" +
"    xmlns:stock=\"http://mycompany.com/stocks\"" +
"    soap:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">" +
"    <Body>" +
"        <stock:GetLastTradePrice>" +
"            <stock:symbol>DIS</stock:symbol>" +
"        </stock:GetLastTradePrice>" +
"    </Body>" +
"</Envelope>";

x = new XML(text);
TEST(4, correct, x);

// Make sure it's a copy
x =
<alpha>
    <bravo>one</bravo>
</alpha>;

y = new XML(x);

x.bravo.prependChild(<charlie>two</charlie>);

correct =
<alpha>
    <bravo>one</bravo>
</alpha>;

TEST(5, correct, y);

// Make text node
x = new XML("4");
TEST_XML(6, "4", x);

x = new XML(4);
TEST_XML(7, "4", x);

// Undefined and null should behave like ""
x = new XML(null);
TEST_XML(8, "", x);

x = new XML(undefined);
TEST_XML(9, "", x);

// see bug 320008
x = new XML("<hello a='\"' />");
TEST_XML(10, "<hello a=\"&quot;\"/>", x);

END();
