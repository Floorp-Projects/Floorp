/**
 *  File Name:          RegExp/octal-002.js
 *  ECMA Section:       15.7.1
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *  Simple test cases for matching OctalEscapeSequences.
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
    var SECTION = "RegExp/octal-002.js";
    var VERSION = "ECMA_2";
    var TITLE   = "RegExp patterns that contain OctalEscapeSequences";
    var BUGNUMBER="http://scopus/bugsplat/show_bug.cgi?id=346189";

    startTest();

// These examples come from 15.7.1, OctalEscapeSequences

/* If n is [8–9], and the decimal value of n is less than or equal to the
 * number of parenthesised subexpressions in the pattern, then \n is a
 * backreference; otherwise \n is the literal character n. For example, in
 * the pattern “(.)(.)(.)(.)(.)(.)(.)(.)\8”, “\8” is a backreference. By
 * contrast, in the pattern “……..\8”, “\8” is the character “8”.
 */

// backreference
    AddRegExpCases(
        /(.)(.)(.)(.)(.)(.)(.)(.)\8/,
        "/(.)(.)(.)(.)(.)(.)(.)(.)\\8",
        "aabbccaaabbbccc",
        "aabbccaaabbbccc",
        0,
        ["aabbccaaa", "a", "a", "b", "b", "c", "c", "a", "a"] );

    AddRegExpCases(
        /(.)(.)(.)(.)(.)(.)(.)(.)(.)\9/,
        "/(.)(.)(.)(.)(.)(.)(.)(.)\\9",
        "aabbccaabbcc",
        "aabbccaabbcc",
        0,
        ["aabbccaabb", "a", "a", "b", "b", "c", "c", "a", "a", "b"] );

    AddRegExpCases(
        /(.)(.)(.)(.)(.)(.)(.)(.)(.)\8/,
        "/(.)(.)(.)(.)(.)(.)(.)(.)(.)\\8",
        "aabbccaababcc",
        "aabbccaababcc",
        0,
        ["aabbccaaba", "a", "a", "b", "b", "c", "c", "a", "a", "b"] );

    AddRegExpCases(
        /(.)(.)(.)(.)(.)(.)(.)(.)\9/,
        "/(.)(.)(.)(.)(.)(.)(.)(.)\\9",
        "aabbccddeeffgghh",
        "aabbccddeeffgghh",
        0,
        null );

// literal

    AddRegExpCases(
        /(.)(.)(.)(.)(.)(.)(.)\8/,
        "/(.)(.)(.)(.)(.)(.)(.)\\8",
        "aabbccaaa87654321",
        "aabbccaaa87654321",
        2,
        ["bbccaaa8", "b", "b", "c", "c", "a", "a", "a"] );

    AddRegExpCases(
        /.......\8/,
        "/.......\\8/",
        "aabbccaaa87654321",
        "aabbccaaa87654321",
        2,
        ["bbccaaa8"]);

    AddRegExpCases(
        /........\8/,
        "/........\\8/",
        "aabbccaaa87654321",
        "aabbccaaa87654321",
        1,
        ["abbccaaa8"]);

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
