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

START("13.5.4.9 - XMLList descendants()");

//TEST(1, true, XMLList.prototype.hasOwnProperty("descendants"));
    
XML.prettyPrinting = false;

var xmlDoc = "<MLB><foo>bar</foo><Team>Giants<foo>bar</foo></Team><City><foo>bar</foo>San Francisco</City><Team>Padres</Team><City>San Diego</City></MLB>";

AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('Team')", "<Team>Giants<foo>bar</foo></Team>" + NL() + "<Team>Padres</Team>", 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('Team').toString()) );
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('Team').length()", 2, 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('Team').length()) );
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('Team') instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('Team') instanceof XMLList) );

// find multiple levels of descendents
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('foo')", "<foo>bar</foo>" + NL() + "<foo>bar</foo>" + NL() + "<foo>bar</foo>", 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('foo').toString()) );
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('foo').length()", 3, 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('foo').length()) );
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('foo') instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('foo') instanceof XMLList) );

// no matching descendents
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('nomatch')", "", 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('nomatch').toString()) );
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('nomatch').length()", 0, 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('nomatch').length()) );
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.descendants('nomatch') instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.descendants('nomatch') instanceof XMLList) );

END();
