/**
 *  File Name:          RegExp/octal-001.js
 *  ECMA Section:       15.7.1
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *  Simple test cases for matching OctalEscapeSequences.
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
    var SECTION = "RegExp/octal-001.js";
    var VERSION = "ECMA_2";
    var TITLE   = "RegExp patterns that contain OctalEscapeSequences";
    var BUGNUMBER="http://scopus/bugsplat/show_bug.cgi?id=346196";

    startTest();

// These examples come from 15.7.1, OctalEscapeSequences

// If n is exactly 1 octal digit other than “0”, and the octal value of n is
// less than or equal to the number of parenthesised subexpressions in the
// pattern, then \n is a backreference; otherwise \n is an octal value. For
// example, in the pattern “(.)\1”, “\1” is a backreference. By contrast, in
// the pattern “.\1”, “\1” is an octal value.

// backreference
    AddRegExpCases(
        /(.)\1/,
        "/(.)\\1/",
        "HI!!",
        "HI!",
        2,
        ["!!", "!"] );

// octal value
    AddRegExpCases(
        /.\1/,
        "/.\\1/",
        "HI!" + String.fromCharCode(1),
        "HI!+ String.fromCharCode(1)",
        2,
        ["!\1"] );

// backreference
    AddRegExpCases( /(.)\041/, "/(.)\\041/", "HI!", "HI!", 1, ["I!", "I"] );

// octal value
    AddRegExpCases( /.\041/, "/.\\041/", "HI!", "HI!", 1, ["I!"] );

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
