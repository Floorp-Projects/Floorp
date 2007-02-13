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
    var SECTION = "LexicalConventions/regexp-literals-001.js";
    var VERSION = "ECMA_2";
    var TITLE   = "Regular Expression Literals";

    startTest();

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
        
    // Regular Expression Literals may not be empty; // should be regarded
    // as a comment, not a RegExp literal.

    s = //;

    "passed";

    array[item++] = new TestCase(SECTION,
        "// should be a comment, not a regular expression literal",
        "passed",
        String(s));

    array[item++] = new TestCase(SECTION,
        "// typeof object should be type of object declared on following line",
        "passed",
        (typeof s) == "string" ? "passed" : "failed" );
    
    array[item++] = new TestCase(SECTION,
        "// should not return an object of the type RegExp",
        "passed",
        (typeof s == "object") ? "failed" : "passed" );

    

    var regexp1:RegExp = /a*b/;
    var regexp2:RegExp = /a*b/;

    array[item++] = new TestCase(SECTION,
        "Two regular expression literals should never compare as === to each other even if the two literals contents are the same",
        "failed",
        (regexp1 === regexp2) ? "passed" : "failed" );

    var regexp3:RegExp = new RegExp();

    

    array[item++] = new TestCase(SECTION,
        "creating RegExp object using new keyword",
        "true",
        ((regexp3 instanceof RegExp).toString()));

    var regexp4:RegExp = RegExp();

    array[item++] = new TestCase(SECTION,
        "creating RegExp object using new keyword",
        "true",
        ((regexp4 instanceof RegExp).toString()));
        
    return array;   
}
