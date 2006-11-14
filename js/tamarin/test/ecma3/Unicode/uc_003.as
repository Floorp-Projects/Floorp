/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Rob Ginda rginda@netscape.com
 *
 *
 * Modified 2/7/2005 by Sushant Dutta (sdutta@macromedia.com)
 *                 "eval" function had to be removed and so the actual values
 *                 had to be modified.   
 */
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
