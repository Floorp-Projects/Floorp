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

START("13.5.4.6 - XMLList comments()");

//TEST(1, true, XMLList.prototype.hasOwnProperty("comments"));

// !!@ very strange that this does find comments in the list but only in elements of the list

var xmlDoc = "<!-- This is Comment --><employee id='1'><!-- This is another Comment --><name>John</name> <city>California</city> </employee><!-- me too -->";

XML.prettyPrinting = true;
XML.ignoreComments = false;
AddTestCase( "XML.ignoreComments = false, MYXML = new XMLList(xmlDoc), MYXML.comments().toString()", 
	"<!-- This is another Comment -->", 
	(XML.ignoreComments = false, MYXML = new XMLList(xmlDoc), MYXML.comments().toString()));
AddTestCase( "XML.ignoreComments = false, MYXML = new XMLList(xmlDoc), MYXML.comments() instanceof XMLList", true, (MYXML = new XMLList(xmlDoc), MYXML.comments() instanceof XMLList));
AddTestCase( "XML.ignoreComments = false, MYXML = new XMLList(xmlDoc), MYXML.comments() instanceof XML", false, (MYXML = new XMLList(xmlDoc), MYXML.comments() instanceof XML));
AddTestCase( "XML.ignoreComments = false, MYXML = new XMLList(xmlDoc), MYXML.comments().length()", 1, (MYXML = new XMLList(xmlDoc), MYXML.comments().length() ));

AddTestCase( "XML.ignoreComments = false, MYXML = new XMLList(xmlDoc), XML.ignoreComments = true, MYXML.comments().toString()", 
	"<!-- This is another Comment -->", 
	(XML.ignoreComments = false, MYXML = new XMLList(xmlDoc), XML.ignoreComments = true, MYXML.comments().toString()));

END();
