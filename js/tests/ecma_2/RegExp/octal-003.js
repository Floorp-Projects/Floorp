/**
 *  File Name:          RegExp/octal-003.js
 *  ECMA Section:       15.7.1
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *  Simple test cases for matching OctalEscapeSequences.
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
    var SECTION = "RegExp/octal-003.js";
    var VERSION = "ECMA_2";
    var TITLE   = "RegExp patterns that contain OctalEscapeSequences";
    var BUGNUMBER="http://scopus/bugsplat/show_bug.cgi?id=346132";

    startTest();

// These examples come from 15.7.1, OctalEscapeSequences

/* If the first digit is [0–3], the octal value may be 1, 2, or 3 digit long.
 * For example, "\11" and "\011" both match “\t”, while "\0111" matches "\t1”.
 * If the first digit is [4--7], the octal value may be 1 or 2 digits long.
 * For example, "\44" matches “$”, while “\444” matches“$4”.
 */

    AddRegExpCases( /\11/,  "/\\11/",  "\t12", "\\t12", 0, ["\t"] );
    AddRegExpCases( /\011/, "/\\011/", "\t12", "\\t12", 0, ["\t"] );
    AddRegExpCases( /\01111/, "/\\01111/","I12", "I12", 0, ["I1"] );

    AddRegExpCases( /\44/,    "/\\44/",  "123$456", "123$456", 3, ["$"] );
    AddRegExpCases( /\444/,    "/\\444/",  "123$456", "123$456", 3, ["$4"] );

    test();

function AddRegExpCases(
    regexp, str_regexp, pattern, str_pattern, index, matches_array ) {

    // prevent a runtime error

    if ( regexp.exec(pattern) == null || matches_array == null ) {
        AddTestCase(
            regexp + ".exec(" + str_pattern +")",
            matches_array,
            regexp.exec(pattern) );

        return;
    }
    AddTestCase(
        str_regexp + ".exec(" + str_pattern +").length",
        matches_array.length,
        regexp.exec(pattern).length );

    AddTestCase(
        str_regexp + ".exec(" + str_pattern +").index",
        index,
        regexp.exec(pattern).index );

    AddTestCase(
        str_regexp + ".exec(" + str_pattern +").input",
        pattern,
        regexp.exec(pattern).input );

    AddTestCase(
        str_regexp + ".exec(" + str_pattern +").toString()",
        matches_array.toString(),
        regexp.exec(pattern).toString() );
/*
    var limit = matches_array.length > regexp.exec(pattern).length
                ? matches_array.length
                : regexp.exec(pattern).length;

    for ( var matches = 0; matches < limit; matches++ ) {
        AddTestCase(
            str_regexp + ".exec(" + str_pattern +")[" + matches +"]",
            matches_array[matches],
            regexp.exec(pattern)[matches] );
    }
*/
}
