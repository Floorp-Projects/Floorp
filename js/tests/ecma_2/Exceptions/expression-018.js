/**
    File Name:          expression-018.js
    Corresponds To:     11.4.1-n.js
    ECMA Section:       11.4.1 the Delete Operator
    Description:        returns true if the property could be deleted
                        returns false if it could not be deleted
    Author:             christine@netscape.com
    Date:               7 july 1997

*/
    var SECTION = "expression-018";
    var VERSION = "JS1_4";
    var TITLE   = "The delete operator";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var tc = 0;
    var testcases = new Array();

    var result = "Failed";
    var exception = "No exception thrown";
    var expect = "Passed";

    try {
        eval("result = delete(x = new Date());");
    } catch ( e ) {
        result = expect;
        exception = e.toString();
    }

    testcases[tc++] = new TestCase(
        SECTION,
        "delete ( x = new Date() )" +
        " (threw " + exception +")",
        expect,
        result );

    test();
