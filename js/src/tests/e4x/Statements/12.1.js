/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("12.1 - Default XML Namespace");


// Declare some namespaces ad a default namespace for the current block
var soap = new Namespace("http://schemas.xmlsoap.org/soap/envelope/");
var stock = new Namespace("http://mycompany.com/stocks");
default xml namespace = soap;

// Create an XML initializer in the default (i.e., soap) namespace
message =
<Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
    soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <Body>
        <stock:GetLastTradePrice xmlns:stock="http://mycompany.com/stocks">
            <stock:symbol>DIS</stock:symbol>
        </stock:GetLastTradePrice>
    </Body>
</Envelope>;

// Extract the soap encoding style using a QualifiedIdentifier
encodingStyle = message.@soap::encodingStyle;
TEST_XML(1, "http://schemas.xmlsoap.org/soap/encoding/", encodingStyle);

// Extract the body from the soap message using the default namespace
correct =
<Body
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
    <stock:GetLastTradePrice xmlns:stock="http://mycompany.com/stocks">
        <stock:symbol>DIS</stock:symbol>
    </stock:GetLastTradePrice>
</Body>;

body = message.Body;
TEST_XML(2, correct.toXMLString(), body);

// Change the stock symbol using the default namespace and qualified names
correct =
<Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
    soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <Body>
        <stock:GetLastTradePrice xmlns:stock="http://mycompany.com/stocks">
            <stock:symbol>MYCO</stock:symbol>
        </stock:GetLastTradePrice>
    </Body>
</Envelope>;

message.Body.stock::GetLastTradePrice.stock::symbol = "MYCO";

TEST(3, correct, message);

function scopeTest()
{
    var x = <a/>;
    TEST(4, soap.uri, x.namespace().uri);

    default xml namespace = "http://" + "someuri.org";
    x = <a/>;
    TEST(5, "http://someuri.org", x.namespace().uri);
}

scopeTest();

x = <a><b><c xmlns="">foo</c></b></a>;
TEST(6, soap.uri, x.namespace().uri);
TEST(7, soap.uri, x.b.namespace().uri);

ns = new Namespace("");
TEST(8, "foo", x.b.ns::c.toString());

x = <a foo="bar"/>;
TEST(9, soap.uri, x.namespace().uri);
TEST(10, "", x.@foo.namespace().uri);
TEST_XML(11, "bar", x.@foo);

default xml namespace = "";
x = <x/>;
ns = new Namespace("http://someuri");
default xml namespace = ns;
x.a = "foo";
TEST(12, "foo", x["a"].toString());
q = new QName("a");
TEST(13, "foo", x[q].toString());

default xml namespace = "";
x[q] = "bar";
TEST(14, "bar", x.ns::a.toString());

END();
