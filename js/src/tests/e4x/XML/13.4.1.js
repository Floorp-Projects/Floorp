/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.1 - XML Constructor as Function");

x = XML();
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

x = XML(correct);
TEST(3, correct, x);

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

x =  XML(text);
TEST(4, correct, x);

// Make sure it's not copied if it's XML
x =
<alpha>
    <bravo>two</bravo>
</alpha>;

y = XML(x);

x.bravo = "three";

correct =
<alpha>
    <bravo>three</bravo>
</alpha>;

TEST(5, correct, y);

// Make text node
x = XML("4");
TEST_XML(6, 4, x);

x = XML(4);
TEST_XML(7, 4, x);

// Undefined and null should behave like ""
x = XML(null);
TEST_XML(8, "", x);

x = XML(undefined);
TEST_XML(9, "", x);
 

END();
