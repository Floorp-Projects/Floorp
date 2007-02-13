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

START("11.2.3 - XML Descendant Accessor");

var e = 
<employees>
    <employee id="1"><name>Joe</name><age>20</age></employee>
    <employee id="2"><name>Sue</name><age>30</age></employee>
</employees>    

names = e..name;

correct =
<name>Joe</name> +
<name>Sue</name>;

TEST(1, correct, names);

e = "<employees><employee id=\"1\"><name>Joe</name><age>20</age></employee><employee id=\"2\"><name>Sue</name><age>30</age></employee></employees>";

AddTestCase("xml..validnode:", "Joe", (x1 = new XML(e), names = x1..name, names[0].toString()));

AddTestCase("xml..validnode length:", 2, (x1 = new XML(e), names = x1..name, names.length()));

AddTestCase("xml..invalidnode:", undefined, (x1 = new XML(e), names = x1..hood, names[0]));

AddTestCase("xmllist..validnode:", "Joe", (x1 = new XMLList(e), names = x1..name, names[0].toString()));

AddTestCase("xmllist..invalidnode:", undefined, (x1 = new XMLList(e), names = x1..hood, names[0]));

AddTestCase("xmllist..invalidnode length:", 0, (x1 = new XMLList(e), names = x1..hood, names.length()));

e = 
<employees>
    <employee id="1"><first_name>Joe</first_name><age>20</age></employee>
    <employee id="2"><first_name>Sue</first_name><age>30</age></employee>
</employees> 

correct =
<first_name>Joe</first_name> +
<first_name>Sue</first_name>;

names = e..first_name;

TEST(2, correct, names);

e = 
<employees>
    <employee id="1"><first-name>Joe</first-name><age>20</age></employee>
    <employee id="2"><first-name>Sue</first-name><age>30</age></employee>
</employees> 

e =
<company><staff>
	<bug attr='1'><coat><bug>heart</bug></coat></bug>
	<bug attr='2'><dirt><bug>part</bug></dirt></bug>
</staff></company>

es = <><bug attr='1'><coat><bug>heart</bug></coat></bug><bug>heart</bug><bug attr='2'><dirt><bug>part</bug></dirt></bug><bug>part</bug></>;

AddTestCase(3, es, e..bug);

END();
