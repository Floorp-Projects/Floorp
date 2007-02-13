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

START("11.2.4 - XML Filtering Predicate Operator");

var p;

e = <employees>
    <employee id="0"><fname>John</fname><age>20</age></employee>
    <employee id="1"><fname>Sue</fname><age>30</age></employee>
    </employees>;


correct = <employee id="0"><fname>John</fname><age>20</age></employee>;

john = e.employee.(fname == "John");
TEST(1, correct, john);    

john = e.employee.(fname == "John");
TEST(2, correct, john);    

correct = 
<employee id="0"><fname>John</fname><age>20</age></employee> +
<employee id="1"><fname>Sue</fname><age>30</age></employee>;

twoEmployees = e.employee.(@id == 0 || @id == 1);
TEST(3, correct, twoEmployees);

twoEmployees = e.employee.(@id == 0 || @id == 1);
TEST(4, correct, twoEmployees);

i = 0;
twoEmployees = new XMLList();
for each (p in e..employee)
{
    if (p.@id == 0 || p.@id == 1)
    {
        twoEmployees += p;
    }
}
TEST(5, correct, twoEmployees);

i = 0;
twoEmployees = new XMLList();
for each (p in e..employee)
{
    if (p.@id == 0 || p.@id == 1)
    {
        twoEmployees[i++] = p;
    }
}
TEST(6, correct, twoEmployees);

// test with syntax
e = <employees>
    <employee id="0"><fname>John</fname><age>20</age></employee>
    <employee id="1"><fname>Sue</fname><age>30</age></employee>
    </employees>;

correct = 
<employee id="0"><fname>John</fname><age>20</age></employee> +
<employee id="1"><fname>Sue</fname><age>30</age></employee>;

i = 0;
twoEmployees = new XMLList();
for each (p in e..employee)
{
    with (p)
    {
        if (@id == 0 || @id == 1)
        {
            twoEmployees[i++] = p;
        }
    }
}
TEST(7, correct, twoEmployees);

var xml = "<employees><employee id=\"1\"><fname>Joe</fname><age>20</age></employee><employee id=\"2\"><fname>Sue</fname><age>Joe</age></employee></employees>";
var e = new XML(xml);

// get employee with fname Joe
AddTestCase("e.employee.(fname == \"Joe\")", 1, (joe = e.employee.(fname == "Joe"), joe.length()));


// employees with id's 0 & 1
AddTestCase("employees with id's 1 & 2", 2, (emps = e.employee.(@id == 1 || @id == 2), emps.length()));


// name of employee with id 1
AddTestCase("name of employee with id 1", "Joe", (emp = e.employee.(@id == 1).fname, emp.toString()));


// get the two employees with ids 0 and 1 using a predicate
var i = 0;
var twoEmployees = new XMLList();
for each (p in e..employee) {
 	with (p) {
	 	if (@id == 1 || @id == 2) {
			twoEmployees[i++] = p;
	  	}
	}
}

var twoEmployees = e..employee.(@id == 1 || @id == 2);

AddTestCase("Compare to equivalent XMLList", true, (emps = e..employee.(@id == 1 || @id == 2), emps == twoEmployees));

 var employees:XML =
<employees>
<employee ssn="123-123-1234" id="1">
<name first="John" last="Doe"/>
<address>
<street>11 Main St.</street>
<city>San Francisco</city>
<state>CA</state>
<zip>98765</zip>
</address>
</employee>
<employee ssn="789-789-7890" id="2">
<name first="Mary" last="Roe"/>
<address>
<street>99 Broad St.</street>
<city>Newton</city>
<state>MA</state>
<zip>01234</zip>
</address>
</employee>
</employees>;

for each (var id:XML in employees.employee.@id) {
trace(id); // 123-123-1234
}

correct = 
<employee ssn="789-789-7890" id="2">
<name first="Mary" last="Roe"/>
<address>
<street>99 Broad St.</street>
<city>Newton</city>
<state>MA</state>
<zip>01234</zip>
</address>
</employee>;

var idToFind:String = "2";
AddTestCase("employees.employee.(@id == idToFind)", correct, (employees.employee.(@id == idToFind)));


END();
