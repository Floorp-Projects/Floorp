/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Ginda rginda@netscape.com
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
    var SECTION = "uc_003";
    var VERSION = "";
    var TITLE = "Escapes in identifiers test.";
    printBugNumber (23608);
    printBugNumber (23607);

    startTest();

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();
    
function getTestCases()
{
    var array = new Array();
    var item = 0;
    
    var \u0041 = 5;
    var A\u03B2 = 15;
    var c\u0061se = 25;


/*
    array[item++] = new TestCase(SECTION, "Escaped ASCII Identifier test.", 5, ("\u0041"));
    array[item++] = new TestCase(SECTION, "Escaped ASCII Identifier test", 6, ("++\u0041"));
    array[item++] = new TestCase(SECTION, "Escaped non-ASCII Identifier test", 15, ("A\u03B2"));
    array[item++] = new TestCase(SECTION, "Escaped non-ASCII Identifier test", 16, ("++A\u03B2"));
    array[item++] = new TestCase(SECTION, "Escaped keyword Identifier test", 25, ("c\\u00" + "61se"));
    array[item++] = new TestCase(SECTION, "Escaped keyword Identifier test", 26, ("++c\\u00" + "61se"));
 */
                   
    array[item++] = new TestCase(SECTION, "Escaped ASCII Identifier test.", 5, (\u0041));
    array[item++] = new TestCase(SECTION, "Escaped ASCII Identifier test", 6, (++\u0041));
    array[item++] = new TestCase(SECTION, "Escaped non-ASCII Identifier test", 15, (A\u03B2));
    array[item++] = new TestCase(SECTION, "Escaped non-ASCII Identifier test", 16, (++A\u03B2));
    array[item++] = new TestCase(SECTION, "Escaped keyword Identifier test", 25, (c\u0061se));
    array[item++] = new TestCase(SECTION, "Escaped keyword Identifier test", 26, (++c\u0061se));
    
    return array;
}
