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
