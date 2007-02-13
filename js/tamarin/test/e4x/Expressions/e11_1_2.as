/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *   Ethan Hugg
 *   Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

START("11.1.2 - Qualified Identifiers");

x1 = 
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

encodingStyle = x1.@soap::encodingStyle;
TEST_XML(1, "http://schemas.xmlsoap.org/soap/encoding/", encodingStyle);

correct = 
<soap:Body xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
    <m:getLastTradePrice xmlns:m="http://mycompany.com/stocks">
        <symbol>DIS</symbol>
    </m:getLastTradePrice>
</soap:Body>;

body = x1.soap::Body;
TEST_XML(2, correct.toXMLString(), body);

body = x1.soap::["Body"];
TEST_XML(3, correct.toXMLString(), body);

q = new QName(soap, "Body");
body = x1[q];
TEST_XML(4, correct.toXMLString(), body);

correct = 
<symbol xmlns:m="http://mycompany.com/stocks" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">MYCO</symbol>;

x1.soap::Body.stock::getLastTradePrice.symbol = "MYCO";
TEST_XML(5, correct.toXMLString(), x1.soap::Body.stock::getLastTradePrice.symbol);

// SOAP messages
var msg1 = <s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
		s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	<s:Body>
		<m:GetLastTradePrice xmlns:m="http://mycompany.com/stocks/">
			<symbol>DIS</symbol>
		</m:GetLastTradePrice>
	</s:Body>
</s:Envelope>

var msg2 = <s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
		s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	<s:Body>
		<m:GetLastTradePrice xmlns:m="http://mycompany.com/stocks/">
			<symbol>MACR</symbol>
		</m:GetLastTradePrice>
	</s:Body>
</s:Envelope>

var msg3 = <s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
		s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	<s:Body>
		<m:GetLastTradePrice xmlns:m="http://mycompany.com/stocks/"
		 m:blah="http://www.hooping.org">
			<symbol>MACR</symbol>
		</m:GetLastTradePrice>
	</s:Body>
</s:Envelope>

var msg4 = <soap>
	<bakery>
		<m:g xmlns:m="http://macromedia.com/software/central/"
		pea="soup"
		pill="box"
		neck="lace"
		m:blah="http://www.hooping.org">
			<pill>box</pill>
			<neck>lace</neck>
			<pea>soup</pea>
		</m:g>
	</bakery>
</soap>

var msg5 = "soupboxlacehttp://www.hooping.org";

// declare namespaces
var ns1 = new Namespace("http://schemas.xmlsoap.org/soap/envelope/");
var ns2= new Namespace ("http://mycompany.com/stocks/");
var ns3= new Namespace ("http://macromedia.com/software/central/");

// extract the soap encoding style and body from the soap msg1
var encodingStyle = msg1.@ns1::encodingStyle;
var stockURL = msg1.ns1::Body.ns2::GetLastTradePrice.@ns2::blah;

var body = msg1.ns1::Body;

// change the stock symbol
body.ns2::GetLastTradePrice.symbol = "MACR";


AddTestCase("body.ns2::GetLastTradePrice.symbol = \"MACR\"", "MACR",
           ( body.ns2::GetLastTradePrice.symbol.toString()));


bodyQ = msg1[QName(ns1, "Body")];

AddTestCase("ms1.ns1::Body == msg1[QName(ns1, \"Body\")]", true, (bodyQ == body));

AddTestCase("msg1 == msg2", true,
           ( msg1 == msg2));
           
AddTestCase("msg1.@ns1::encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/",
           ( msg1.@ns1::encodingStyle.toString()));

AddTestCase("msg3.ns1::Body.ns2::GetLastTradePrice.@ns2", "http://www.hooping.org",
           ( msg3.ns1::Body.ns2::GetLastTradePrice.@ns2::blah.toString()));


// Rhino behaves differently:

AddTestCase("msg4.bakery.ns3::g.@*", msg5,
           ( msg4.bakery.ns3::g.@*.toString()));
           
var x1 = <x xmlns:ns="foo" ns:v='55'><ns:a>10</ns:a><b/><ns:c/></x>;
var ns = new Namespace("foo");

AddTestCase("x1.ns::*", new XMLList("<ns:a xmlns:ns=\"foo\">10</ns:a><ns:c xmlns:ns=\"foo\"/>").toString(), x1.ns::*.toString());

AddTestCase("x1.ns::a", "10", x1.ns::a.toString())

AddTestCase("x1.*::a", "10", x1.*::a.toString()); // issue: assert

AddTestCase("x1.ns::a", "20", (x1.ns::a = 20, x1.ns::a.toString()));

AddTestCase("x1.@ns::['v']", "55", x1.@ns::['v'].toString());

AddTestCase("x1.@ns::['v']", "555", (x1.@ns::['v'] = '555', x1.@ns::['v'].toString()));

var y1 = <y xmlns:ns="foo" a="10" b="20" ns:c="30" ns:d="40"/>
AddTestCase("y1.@ns::*.length()", 2, y1.@ns::*.length());

var z = new XMLList("<b xmlns:ns=\"foo\"/><ns:c xmlns:ns=\"foo\"/>");
AddTestCase("x1.*", z.toString(), (delete x1.ns::a, x1.*.toString()));

END();
