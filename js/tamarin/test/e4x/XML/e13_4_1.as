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

START("13.4.1 - XML Constructor as Function");

x1 = XML();
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

x1 = XML(correct);
TEST(3, correct, x1);

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

x1 =  XML(text);
TEST(4, correct, x1);

// Make sure it's not copied if it's XML
x1 = 
<alpha>
    <bravo>two</bravo>
</alpha>;

y1 = XML(x1);

x1.bravo = "three";

correct = 
<alpha>
    <bravo>three</bravo>
</alpha>;

TEST(5, correct, y1);

// Make text node
x1 = XML("4");
TEST_XML(6, 4, x1);

x1 = XML(4);
TEST_XML(7, 4, x1);
 
// Undefined and null should behave like ""
x1 = XML(null);
TEST_XML(8, "", x1);

x1 = XML(undefined);
TEST_XML(9, "", x1);
  
XML.prettyPrinting = false;

var thisXML = "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>";
var NULL_OBJECT = null;
// value is null
AddTestCase( "XML(null).valueOf().toString()", "", XML(null).valueOf().toString() );
AddTestCase( "typeof XML(null)", "xml", typeof XML(null) );

// value is undefined
AddTestCase( "XML(undefined).valueOf().toString()", "", XML(undefined).valueOf().toString() );
AddTestCase( "typeof XML(undefined)", "xml", typeof XML(undefined) );

// value is not supplied
AddTestCase( "XML().valueOf().toString()", "", XML().valueOf().toString() );
AddTestCase( "typeof XML()", "xml", typeof XML() );

// value is supplied 
AddTestCase( "XML(thisXML).valueOf().toString()", thisXML, XML(thisXML).valueOf().toString() );
AddTestCase( "typeof XML(thisXML)", "xml", typeof XML(thisXML) );

END();
