/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 * Ethan Hugg
 * Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

START("13.4.3.4 - XML.ignoreWhitespace");

// We set this to false so we can more easily compare string output
XML.prettyPrinting = false;

// xml doc with white space
var xmlDoc = "<XML>  <TEAM>Giants</TEAM>\u000D<CITY>San\u0020Francisco</CITY>\u000A<SPORT>Baseball</SPORT>\u0009</XML>"


// a) value of ignoreWhitespace
AddTestCase( "XML.ignoreWhitespace = false, XML.ignoreWhitespace", false, (XML.ignoreWhitespace = false, XML.ignoreWhitespace));
AddTestCase( "XML.ignoreWhitespace = true, XML.ignoreWhitespace", true, (XML.ignoreWhitespace = true, XML.ignoreWhitespace));


// b) whitespace is ignored when true, not ignored when false
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.toString()", "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY><SPORT>Baseball</SPORT></XML>", 
             (XML.ignoreWhitespace = true, MYXML = new XML(xmlDoc), MYXML.toString() ));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.toString() with ignoreWhitespace=false", 
		"<XML>  <TEAM>Giants</TEAM>\u000D<CITY>San\u0020Francisco</CITY>\u000A<SPORT>Baseball</SPORT>\u0009</XML>", 
        (XML.ignoreWhitespace = false, MYXML = new XML(xmlDoc), MYXML.toString() ));

// c) whitespace characters
// tab
xmlDoc = "<a>\t<b>tab</b></a>";
AddTestCase( "XML with tab and XML.ignoreWhiteSpace = true", "<a><b>tab</b></a>",
           (XML.ignoreWhitespace = true, MYXML = new XML(xmlDoc), MYXML.toString() ));

AddTestCase( "XML with tab and XML.ignoreWhiteSpace = false", "<a>\t<b>tab</b></a>",
           (XML.ignoreWhitespace = false, MYXML = new XML(xmlDoc), MYXML.toString() ));

// new line
xmlDoc = "<a>\n<b>\n\ntab</b>\n</a>";
AddTestCase( "XML with new line and XML.ignoreWhiteSpace = true", "<a><b>tab</b></a>",
           (XML.ignoreWhitespace = true, MYXML = new XML(xmlDoc), MYXML.toString() ));

xmlDoc = "<a>\r\n<b>tab</b>\r\n</a>";
AddTestCase( "XML with new line and XML.ignoreWhiteSpace = false", "<a>\r\n<b>tab</b>\r\n</a>",
           (XML.ignoreWhitespace = false, MYXML = new XML(xmlDoc), MYXML.toString() ));

// d) attributes

xmlDoc = "<a><b      attr=\"1      2\">tab</b></a>";
AddTestCase( "new XML(\"<a><a><b      attr='1      2'>tab</b></a>\")", "<a><b attr=\"1      2\">tab</b></a>",
           (XML.ignoreWhitespace = true, MYXML = new XML(xmlDoc), MYXML.toString() ));
           
AddTestCase( "new XML(\"<a><a><b      attr='1      2'>tab</b></a>\")", "<a><b attr=\"1      2\">tab</b></a>",
           (XML.ignoreWhitespace = false, MYXML = new XML(xmlDoc), MYXML.toString() ));


END();
