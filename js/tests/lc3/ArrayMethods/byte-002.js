/* The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation. All Rights Reserved.
 * 
 */
/**
 *  java array objects "inherit" JS string methods.  verify that byte arrays
 *  can inherit JavaScript Array object methods
 *
 *
 */
    var SECTION = "java array object inheritance JavaScript Array methods";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 " + SECTION;

    startTest();

    var b = new java.lang.String("abcdefghijklmnopqrstuvwxyz").getBytes();

    testcases[tc++] = new TestCase(
        "var b = new java.lang.String(\"abcdefghijklmnopqrstuvwxyz\").getBytes(); b.valueOf()",
        b,
        b.valueOf() );

    test();