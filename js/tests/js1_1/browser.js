/*
* The contents of this file are subject to the Netscape Public License
* Version 1.0 (the "NPL"); you may not use this file except in
* compliance with the NPL.  You may obtain a copy of the NPL at
* http://www.mozilla.org/NPL/
*
* Software distributed under the NPL is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
* for the specific language governing rights and limitations under the
* NPL.
*
* The Initial Developer of this code under the NPL is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.  All Rights
* Reserved.
*/

/*
 * JavaScript test library shared functions file for running the tests
 * in the browser.  Overrides the shell's print function with document.write
 * and make everything HTML pretty.
 *
 * To run the tests in the browser, use the mkhtml.pl script to generate
 * html pages that include the shell.js, browser.js (this file), and the
 * test js file in script tags.
 *
 * The source of the page that is generated should look something like this:
 *      <script src="./../shell.js"></script>
 *      <script src="./../browser.js"></script>
 *      <script src="./mytest.js"></script>
 */

onerror = err;

function startTest() {
    if ( BUGNUMBER ) {
        writeLineToLog ("BUGNUMBER: " + BUGNUMBER );
    }

    testcases = new Array();
    tc = 0;
}

function writeLineToLog( string ) {
    document.write( string + "<br>\n");
}
function writeHeaderToLog( string ) {
    document.write( "<h2>" + string + "</h2>" );
}
function stopTest() {
    var gc;
    if ( gc != undefined ) {
        gc();
    }
    document.write( "<hr>" );
}
function writeFormattedResult( expect, actual, string, passed ) {
    var s = "<tt>"+ string ;
    s += "<b>" ;
    s += ( passed ) ? "<font color=#009900> &nbsp;" + PASSED
                    : "<font color=#aa0000>&nbsp;" +  FAILED + expect + "</tt>";
    writeLineToLog( s + "</font></b></tt>" );
    return passed;
}
function err( msg, page, line ) {
    writeLineToLog( "Test failed with the message: " + msg );

    testcases[tc].actual = "error";
    testcases[tc].reason = msg;
    writeTestCaseResult( testcases[tc].expect,
                         testcases[tc].actual,
                         testcases[tc].description +" = "+ testcases[tc].actual +
                         ": " + testcases[tc].reason );
    stopTest();
    return true;
}
