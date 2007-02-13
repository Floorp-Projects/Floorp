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

START("13.4.2 - XML Constructor");

x1 = new XML();
TEST(1, "xml", typeof(x1));
TEST(2, true, x1 instanceof XML);

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

x1 = new XML(correct);
TEST_XML(3, correct.toXMLString(), x1);

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

x1 = new XML(text);
TEST(4, correct, x1);

// Make sure it's a copy
x1 =
<alpha>
    <bravo>one</bravo>
</alpha>;

y1 = new XML(x1);

x1.bravo.prependChild(<charlie>two</charlie>);

correct =
<alpha>
    <bravo>one</bravo>
</alpha>;

TEST(5, correct, y1);

// Make text node
x1 = new XML("4");
TEST_XML(6, "4", x1);

x1 = new XML(4);
TEST_XML(7, "4", x1);

// Undefined and null should behave like ""
x1 = new XML(null);
TEST_XML(8, "", x1);

x1 = new XML(undefined);
TEST_XML(9, "", x1);

var thisXML = "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>";

// value is null
AddTestCase( "typeof new XML(null)", "xml", typeof new XML(null) );
AddTestCase( "new XML(null) instanceof XML", true, new XML(null) instanceof XML);
AddTestCase( "(new XML(null).nodeKind())", "text", (new XML(null)).nodeKind());
AddTestCase( "MYOB = new XML(null); MYOB.toString()", "",
             (MYOB = new XML(null), MYOB.toString(), MYOB.toString()) );

// value is undefined
AddTestCase( "typeof new XML(undefined)", "xml", typeof new XML(undefined) );
AddTestCase( "new XML(undefined) instanceof XML", true, new XML(undefined) instanceof XML);
AddTestCase( "(new XML(undefined).nodeKind())", "text", (new XML(undefined)).nodeKind());
AddTestCase( "MYOB = new XML(undefined); MYOB.toString()", "",
             (MYOB = new XML(undefined), MYOB.toString(), MYOB.toString()) );

// value is not supplied
AddTestCase( "typeof new XML()", "xml", typeof new XML() );
AddTestCase( "new XML() instanceof XML", true, new XML() instanceof XML);
AddTestCase( "(new XML().nodeKind())", "text", (new XML()).nodeKind());
AddTestCase( "MYOB = new XML(); MYOB.toString()", "",
             (MYOB = new XML(), MYOB.toString(), MYOB.toString()) );

//value is a number
AddTestCase( "typeof new XML(5)", "xml", typeof new XML(5) );
AddTestCase( "new XML(5) instanceof XML", true, new XML(5) instanceof XML);
AddTestCase( "(new XML(5).nodeKind())", "text", (new XML(5)).nodeKind());
AddTestCase( "MYOB = new XML(5); MYOB.toString()", "5",
             (MYOB = new XML(5), MYOB.toString(), MYOB.toString()) );

//value is 

// value is supplied 
XML.prettyPrinting = false;
AddTestCase( "typeof new XML(thisXML)", "xml", typeof new XML(thisXML) );
AddTestCase( "new XML(thisXML) instanceof XML", true, new XML(thisXML) instanceof XML);
AddTestCase( "MYOB = new XML(thisXML); MYOB.toString()", "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>",
             (MYOB = new XML(thisXML), MYOB.toString(), MYOB.toString()) );
             
// Strongly typed XML objects
var MYXML1:XML = new XML(thisXML);
AddTestCase("Strongly typed XML object", new XML(thisXML).toString(), MYXML1.toString());

var MYXML2:XML = new XML(<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>);
AddTestCase("var MYXML:XML = new XML(<x><a>b</a><c>d</c></x>);", new XML(thisXML).toString(), MYXML2.toString());

var MYXML3:XML = <XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>;
AddTestCase("var MYXML:XML = <x><a>b</a><c>d</c></x>;", new XML(thisXML).toString(), MYXML3.toString());

var MYXML4:XML = new XML();
AddTestCase("var MYXML:XML = new XML()", new XML(), MYXML4);

var MYXML5:XML = new XML(5);
AddTestCase("var MYXML:XML = new XML(5)", "5", MYXML5.toString());

var a = new XML("<?xml version='1.0' encoding='UTF-8'?><?xml-stylesheet href='mystyle.css' type='text/css'?> <rdf>compiled</rdf>");

AddTestCase("XML with PI and comments", "compiled", a.toString());

try { 
	var b = new XML("<a/><b/>"); 
	result = b;
} catch(e1) {
	result = typeError(e1.toString());
}

AddTestCase("new XML(\"<a/><b/>\")", "TypeError: Error #1088", result);

END();
