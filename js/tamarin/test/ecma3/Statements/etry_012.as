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
    var SECTION = "try-012";
    var VERSION = "ECMA_2";
    var TITLE   = "The try statement";
    var BUGNUMBER="336872";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);


    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
        
    // Tests start here.

    TrySomething( "x = \"hi\"", true );
    TrySomething2( "throw \"boo\"", true );
    TrySomething3( "throw 3", true );



    // x = "hi"
    function TrySomething( expression, throwing ) {
        innerFinally = "FAIL: DID NOT HIT INNER FINALLY BLOCK";
        if (throwing) {
            outerCatch = "FAILED: NO EXCEPTION CAUGHT";
        } else {
            outerCatch = "PASS";
        }
        outerFinally = "FAIL: DID NOT HIT OUTER FINALLY BLOCK";


        // If the inner finally does not throw an exception, the result
        // of the try block should be returned.  (Type of inner return
        // value should be throw if finally executes correctly

        try {
            try {
                throw 0;
            } finally {
                innerFinally = "PASS";
		x = hi;
            }
        } catch ( e  ) {
            if (throwing) {
                outerCatch = "PASS";
            } else {
                outerCatch = "FAIL: HIT OUTER CATCH BLOCK";
            }
        } finally {
            outerFinally = "PASS";
        }


        array[item++] = new TestCase(
                SECTION,
                expression+" evaluated inner finally block",
                "PASS",
                innerFinally );
        array[item++] = new TestCase(
                SECTION,
                expression +" evaluated outer catch block ",
                "PASS",
                outerCatch );
        array[item++] = new TestCase(
                SECTION,
                expression +" evaluated outer finally block",
                "PASS",
                outerFinally );
    }

    function TrySomething2( expression, throwing ) {
        innerFinally = "FAIL: DID NOT HIT INNER FINALLY BLOCK";
        if (throwing) {
            outerCatch = "FAILED: NO EXCEPTION CAUGHT";
        } else {
            outerCatch = "PASS";
        }
        outerFinally = "FAIL: DID NOT HIT OUTER FINALLY BLOCK";


        // If the inner finally does not throw an exception, the result
        // of the try block should be returned.  (Type of inner return
        // value should be throw if finally executes correctly

        try {
            try {
                throw 0;
            } finally {
                innerFinally = "PASS";
		throw "boo";
            }
        } catch ( e  ) {
            if (throwing) {
                outerCatch = "PASS";
            } else {
                outerCatch = "FAIL: HIT OUTER CATCH BLOCK";
            }
        } finally {
            outerFinally = "PASS";
        }


        array[item++] = new TestCase(
                SECTION,
                expression +" evaluated inner finally block",
                "PASS",
                innerFinally );
        array[item++] = new TestCase(
                SECTION,
                expression +" evaluated outer catch block ",
                "PASS",
                outerCatch );
        array[item++] = new TestCase(
                SECTION,
                expression +" evaluated outer finally block",
                "PASS",
                outerFinally );
    }

    function TrySomething3( expression, throwing ) {
        innerFinally = "FAIL: DID NOT HIT INNER FINALLY BLOCK";
        if (throwing) {
            outerCatch = "FAILED: NO EXCEPTION CAUGHT";
        } else {
            outerCatch = "PASS";
        }
        outerFinally = "FAIL: DID NOT HIT OUTER FINALLY BLOCK";


        // If the inner finally does not throw an exception, the result
        // of the try block should be returned.  (Type of inner return
        // value should be throw if finally executes correctly

        try {
            try {
                throw 0;
            } finally {
                innerFinally = "PASS";
		throw 3;
            }
        } catch ( e  ) {
            if (throwing) {
                outerCatch = "PASS";
            } else {
                outerCatch = "FAIL: HIT OUTER CATCH BLOCK";
            }
        } finally {
            outerFinally = "PASS";
        }


        array[item++] = new TestCase(
                SECTION,
                expression +" evaluated inner finally block",
                "PASS",
                innerFinally );
        array[item++] = new TestCase(
                SECTION,
                expression +" evaluated outer catch block ",
                "PASS",
                outerCatch );
        array[item++] = new TestCase(
                SECTION,
                expression +" evaluated outer finally block",
                "PASS",
                outerFinally );
    }

    return array;
}
