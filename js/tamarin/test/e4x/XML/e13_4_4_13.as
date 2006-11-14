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

START("13.4.4.13 - XML elements()");

//TEST(1, true, XML.prototype.hasOwnProperty("elements"));

var xmlDoc = "<MLB><Team>Giants</Team><City>San Francisco</City><Team>Padres</Team><City>San Diego</City></MLB>";

// !!@ need to verify results of these test cases
// !!@ elements appears to be broken in Rhino

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.elements('Team').toString()", "<Team>Giants</Team>" + NL() + "<Team>Padres</Team>", 
             (MYXML = new XML(xmlDoc), MYXML.elements('Team').toString()) );

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.elements('TEAM').toString()", "", 
             (MYXML = new XML(xmlDoc), MYXML.elements('TEAM').toString()) );

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.elements('bogus').toString()", "", 
             (MYXML = new XML(xmlDoc), MYXML.elements('bogus').toString()) );

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.elements()", "<Team>Giants</Team>" + NL() + "<City>San Francisco</City>" + NL() + "<Team>Padres</Team>" + NL() + "<City>San Diego</City>", 
             (MYXML = new XML(xmlDoc), MYXML.elements().toString()) );
    
xmlDoc = "<MLB><Team>Giants</Team><City>San Francisco</City></MLB>";

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.elements('City').toString()", "San Francisco", 
             (MYXML = new XML(xmlDoc), MYXML.elements('City').toString()) );

xmlDoc = "<A><MLB><Team>Giants</Team><City>San Francisco</City><Team>Padres</Team><City>San Diego</City></MLB></A>";

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.elements('MLB')", "<MLB>" + NL() + "  <Team>Giants</Team>" + NL() + "  <City>San Francisco</City>" + NL() + "  <Team>Padres</Team>" + NL() + "  <City>San Diego</City>" + NL() + "</MLB>", 
             (MYXML = new XML(xmlDoc), MYXML.elements('MLB').toString()) );


END();
