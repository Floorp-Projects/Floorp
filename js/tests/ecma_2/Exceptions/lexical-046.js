/**
    File Name:          lexical-046.js
    Corresponds To:     7.7.3-6-n.js
    ECMA Section:       7.7.3 Numeric Literals

    Description:

    This is a regression test for
    http://scopus.mcom.com/bugsplat/show_bug.cgi?id=122884

    Waldemar's comments:

    A numeric literal that starts with either '08' or '09' is interpreted as a
    decimal literal; it should be an error instead.  (Strictly speaking, according
    to ECMA v1 such literals should be interpreted as two integers -- a zero
    followed by a decimal number whose first digit is 8 or 9, but this is a bug in
    ECMA that will be fixed in v2.  In any case, there is no place in the grammar
    where two consecutive numbers would be legal.)

    Author:             christine@netscape.com
    Date:               15 june 1998

*/
    var SECTION = "lexical-046";
    var VERSION = "JS1_4";
    var TITLE   = "Numeric Literals";
    var BUGNUMBER="122884";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var tc = 0;
    var testcases = new Array();

    var result = "Failed";
    var exception = "No exception thrown";
    var expect = "Passed";

    try {
        eval("079");
    } catch ( e ) {
        result = expect;
        exception = e.toString();
    }

    testcases[tc++] = new TestCase(
        SECTION,
        "079" +
        " (threw " + exception +")",
        expect,
        result );

    test();

