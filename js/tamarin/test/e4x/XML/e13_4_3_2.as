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

START("13.4.3.2 - XML.ignoreComments");

var thisXML = "<XML><!--comment1--><TEAM>Giants</TEAM><CITY>San Francisco</CITY><!--comment2--></XML>"

XML.prettyPrinting = false;

// a) initial value of ignoreComments is true 

AddTestCase( "XML.ignoreComments", true, XML.ignoreComments);

// toggling value works ok
AddTestCase( "XML.ignoreComments = false, XML.ignoreComments", false, (XML.ignoreComments = false, XML.ignoreComments));
AddTestCase( "XML.ignoreComments = true, XML.ignoreComments", true, (XML.ignoreComments = true, XML.ignoreComments));

// b) if ignoreComments is true, XML comments are ignored when construction the new XML objects
AddTestCase( "MYXML = new XML(thisXML), MYXML.toString()", "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>", 
             (XML.ignoreComments = true, MYXML = new XML(thisXML), MYXML.toString() ));
AddTestCase( "MYXML = new XML(thisXML), MYXML.toString() with ignoreComemnts=false", "<XML><!--comment1--><TEAM>Giants</TEAM><CITY>San Francisco</CITY><!--comment2--></XML>", 
             (XML.ignoreComments = false, MYXML = new XML(thisXML), MYXML.toString() ));

// If ignoreComments is true, XML constructor from another XML node ignores comments
XML.ignoreComments = false;
var MYXML = new XML(thisXML); // this XML node has comments
XML.ignoreComments = true;
var xml2 = new XML(MYXML); // this XML tree should not have comments
AddTestCase( "xml2 = new XML(MYXML), xml2.toString()", "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>", 
             (xml2.toString()) );
XML.ignoreComments = false;
var xml3 = new XML(MYXML); // this XML tree will have comments
AddTestCase( "xml3 = new XML(MYXML), xml3.toString()", "<XML><!--comment1--><TEAM>Giants</TEAM><CITY>San Francisco</CITY><!--comment2--></XML>", 
             (xml3.toString()) );


// c) two attributes { DontEnum, DontDelete }
// !!@


END();
