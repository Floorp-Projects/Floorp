/**
 *  File Name:
 *  ECMA Section:
 *  Description:
 *
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
    var SECTION = "";
    var VERSION = "ECMA_2";
    var TITLE   = "Keywords";

    startTest();

    // Regular Expression Literals may not be empty; // should be regarded
    // as a comment, not a RegExp literal.

    var result = "passed";

    super;

    AddTestCase(
        "using the expression \"super\" shouldn't cause js to crash",
        "passed" ,
        result );

    test();
