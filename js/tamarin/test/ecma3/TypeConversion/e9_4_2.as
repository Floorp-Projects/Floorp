/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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
    var SECTION = "9.4-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "ToInteger";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // some special cases
	td = new Date(Number.NaN);
    array[item++] = new TestCase( SECTION,  "td = new Date(Number.NaN); td.valueOf()",  Number.NaN, td.valueOf() );
    td = new Date(Infinity); 
    array[item++] = new TestCase( SECTION,  "td = new Date(Infinity); td.valueOf()",    Number.NaN, td.valueOf() );
    td = new Date(-Infinity); 
    array[item++] = new TestCase( SECTION,  "td = new Date(-Infinity); td.valueOf()",   Number.NaN, td.valueOf() );
    td = new Date(-0);
    array[item++] = new TestCase( SECTION,  "td = new Date(-0); td.valueOf()",          -0,         td.valueOf() );
    td = new Date(0);
    array[item++] = new TestCase( SECTION,  "td = new Date(0); td.valueOf()",           0,          td.valueOf() );

    // value is not an integer
	td = new Date(3.14159);
    array[item++] = new TestCase( SECTION,  "td = new Date(3.14159); td.valueOf()",     3,          td.valueOf() );
    td = new Date(Math.PI);
    array[item++] = new TestCase( SECTION,  "td = new Date(Math.PI); td.valueOf()",     3,          td.valueOf() );
    td = new Date(-Math.PI);
    array[item++] = new TestCase( SECTION,  "td = new Date(-Math.PI);td.valueOf()",     -3,         td.valueOf() );
    td = new Date(3.14159e2);
    array[item++] = new TestCase( SECTION,  "td = new Date(3.14159e2); td.valueOf()",   314,        td.valueOf());
	td = new Date(.692147e1);
    array[item++] = new TestCase( SECTION,  "td = new Date(.692147e1); td.valueOf()",   6,          td.valueOf() );
    td = new Date(-.692147e1);
    array[item++] = new TestCase( SECTION,  "td = new Date(-.692147e1);td.valueOf()",   -6,         td.valueOf() );

    // value is not a number
	td = new Date(true);
    array[item++] = new TestCase( SECTION,  "td = new Date(true); td.valueOf()",        1,          td.valueOf() );
    td = new Date(false);
    array[item++] = new TestCase( SECTION,  "td = new Date(false); td.valueOf()",       0,          td.valueOf());
	td = new Date(new Number(Math.PI));
    array[item++] = new TestCase( SECTION,  "td = new Date(new Number(Math.PI)); td.valueOf()",  3, td.valueOf() );
    td = new Date(new Number(Math.PI));
    array[item++] = new TestCase( SECTION,  "td = new Date(new Number(Math.PI)); td.valueOf()",  3, td.valueOf() );

    // edge cases
    td = new Date(8.64e15);
    array[item++] = new TestCase( SECTION,  "td = new Date(8.64e15); td.valueOf()",     8.64e15,    td.valueOf() );
    td = new Date(-8.64e15);
    array[item++] = new TestCase( SECTION,  "td = new Date(-8.64e15); td.valueOf()",    -8.64e15,   td.valueOf() );
    td = new Date(8.64e-15);
    array[item++] = new TestCase( SECTION,  "td = new Date(8.64e-15); td.valueOf()",    0,          td.valueOf() );
    td = new Date(-8.64e-15);
    array[item++] = new TestCase( SECTION,  "td = new Date(-8.64e-15); td.valueOf()",   0,          td.valueOf() );

    return ( array );
}
