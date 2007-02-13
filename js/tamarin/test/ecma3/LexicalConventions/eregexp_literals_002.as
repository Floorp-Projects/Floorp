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
    var SECTION = "LexicalConventions/regexp-literals-002.js";
    var VERSION = "ECMA_2";
    var TITLE   = "Regular Expression Literals";

    startTest();
    var testcases = getTestCases();

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    // A regular expression literal represents an object of type RegExp.

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/x*/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/x*y/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/xy*yz/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/.{1}/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/.{3,4}/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/ab{0,}bc/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/xy+yz/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/xy{1,}yz/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/xy{1,3}yz/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/xy?yz/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/$/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/x.*y/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/x[yz]m/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/x[^yz]m/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/x[^-z]m/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/x\yz\y/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/a\Sb/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/\d/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/\D/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/xy|yz/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/()ef/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/a\(b/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/a\\b/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(a)b(c)/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/a+b+c/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/a.+?c/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(a+|b*)/ instanceof RegExp).toString() );


    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(a+|b){0,}/ instanceof RegExp).toString() );

       
    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(a+|b)+/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(a+|b)?/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(a+|b){0,1}/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/[^ab]*/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/([abc])*d/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/a|b|c|d|e/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(a|b|c|d|e)f/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/[a-zA-Z_][a-zA-Z0-9_]*/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/^a(bc+|b[eh])g|.h$/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(bc+d$|ef*g.|h?i(j|k))/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/((((((((((a))))))))))\10/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/(.*)c(.*)/ instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/abc/i instanceof RegExp).toString() );

    array[item++] = new TestCase(SECTION,
        "// A regular expression literal represents an object of type RegExp.",
        "true",
        (/ab{0,1}?c/i instanceof RegExp).toString() );
        
    return array;
}
