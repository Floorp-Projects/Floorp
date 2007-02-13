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

START("13.4.4.10 - XML contains()");

//TEST(1, true, XML.prototype.hasOwnProperty("contains"));

emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST(2, true, emps.contains(emps));

var xmlDoc = "<MLB><Team>Giants</Team><City>San Francisco</City><Team>Padres</Team><City>San Diego</City></MLB>";

// same object, returns true
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.contains(MYMXL)", true, 
             (MYXML = new XML(xmlDoc), MYXML.contains(MYXML)) );
             
// duplicated object, returns true
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.contains(MYMXL.copy())", true, 
             (MYXML = new XML(xmlDoc), MYXML.contains(MYXML.copy())) );
             
// identical objects, returns true
AddTestCase( "MYXML = new XML(xmlDoc), MYXML2 = new XML(xmlDoc), MYXML.contains(MYMXL2)", true, 
             (MYXML = new XML(xmlDoc), MYXML2 = new XML(xmlDoc), MYXML.contains(MYXML2)) );
             
// identical objects, returns true
AddTestCase( "MYXML = new XML(xmlDoc), MYXML2 = new XML(xmlDoc), MYXML2.contains(MYMXL)", true, 
             (MYXML = new XML(xmlDoc), MYXML2 = new XML(xmlDoc), MYXML2.contains(MYXML)) );
             
// slightly different objects, returns false
AddTestCase( "MYXML = new XML(xmlDoc), MYXML2 = new XML(xmlDoc), MYXML2.foo = 'bar', MYXML.contains(MYMXL2)", false, 
             (MYXML = new XML(xmlDoc), MYXML2 = new XML(xmlDoc), MYXML2.foo = 'bar', MYXML.contains(MYXML2)) );

// slightly different objects #2, returns false
AddTestCase( "MYXML = new XML(xmlDoc), MYXML2 = new XML(xmlDoc), MYXML2.Team[0].foo = 'bar', MYXML.contains(MYMXL2)", false, 
             (MYXML = new XML(xmlDoc), MYXML2 = new XML(xmlDoc), MYXML2.Team[0].foo = 'bar', MYXML.contains(MYXML2)) );

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.Team[0].contains('Giants')", false, 
             (MYXML = new XML(xmlDoc), MYXML.Team[0].contains('Giants')) );
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.Team[1].contains('Giants')", false, 
             (MYXML = new XML(xmlDoc), MYXML.Team[1].contains('Giants')) );
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.City.contains('Giants')", false, 
             (MYXML = new XML(xmlDoc), MYXML.City.contains('Giants')) );

END();
