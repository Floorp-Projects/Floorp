// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.1.2 - Qualified Identifiers");

x =
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
    soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <soap:Body>
        <m:getLastTradePrice xmlns:m="http://mycompany.com/stocks">
            <symbol>DIS</symbol>
        </m:getLastTradePrice>
    </soap:Body>
</soap:Envelope>;   

soap = new Namespace("http://schemas.xmlsoap.org/soap/envelope/");
stock = new Namespace("http://mycompany.com/stocks");

encodingStyle = x.@soap::encodingStyle;
TEST_XML(1, "http://schemas.xmlsoap.org/soap/encoding/", encodingStyle);

correct =
<soap:Body xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
    <m:getLastTradePrice xmlns:m="http://mycompany.com/stocks">
        <symbol>DIS</symbol>
    </m:getLastTradePrice>
</soap:Body>;

body = x.soap::Body;
TEST_XML(2, correct.toXMLString(), body);

body = x.soap::["Body"];
TEST_XML(3, correct.toXMLString(), body);

q = new QName(soap, "Body");
body = x[q];
TEST_XML(4, correct.toXMLString(), body);

correct =
<symbol xmlns:m="http://mycompany.com/stocks" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">MYCO</symbol>;

x.soap::Body.stock::getLastTradePrice.symbol = "MYCO";
TEST_XML(5, correct.toXMLString(), x.soap::Body.stock::getLastTradePrice.symbol);

END();
