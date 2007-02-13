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

START("13.4.3.3 - XML.ignoreProcessingInstructions");

// We set this to false so we can more easily compare string output
XML.prettyPrinting = false;

// xml string with processing instructions
var xmlDoc = "<?xml version=\"1.0\"?><XML><?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>"

// a) value of ignoreProcessingInstructions
AddTestCase ("XML.ignoreProcessingInstructions", true, (XML.ignoreProcessingInstructions));
AddTestCase( "XML.ignoreProcessingInstructions = false, XML.ignoreProcessingInstructions", false, (XML.ignoreProcessingInstructions = false, XML.ignoreProcessingInstructions));
AddTestCase( "XML.ignoreProcessingInstructions = true, XML.ignoreProcessingInstructions", true, (XML.ignoreProcessingInstructions = true, XML.ignoreProcessingInstructions));

// b) if ignoreProcessingInstructions is true, XML processing instructions are ignored when construction the new XML objects
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.toString()", "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>", 
             (XML.ignoreProcessingInstructions = true, MYXML = new XML(xmlDoc), MYXML.toString() ));
             
// !!@ note that the "<?xml version=\"1.0\"?>" tag magically disappeared.             
             
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.toString() with ignoreProcessingInstructions=false", 
		"<XML><?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>", 
        (XML.ignoreProcessingInstructions = false, MYXML = new XML(xmlDoc), MYXML.toString() ));


// If ignoreProcessingInstructions is true, XML constructor from another XML node ignores processing instructions
XML.ignoreProcessingInstructions = false;
var MYXML = new XML(xmlDoc); // this XML node has processing instructions
XML.ignoreProcessingInstructions = true;
var xml2 = new XML(MYXML); // this XML tree should not have processing instructions
AddTestCase( "xml2 = new XML(MYXML), xml2.toString()", "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>", 
             (xml2.toString()) );
XML.ignoreProcessingInstructions = false;
var xml3 = new XML(MYXML); // this XML tree will have processing instructions
AddTestCase( "xml3 = new XML(MYXML), xml3.toString()", 
		"<XML><?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>",
             (xml3.toString()) );


END();
