/**
 *  File Name:          String/split-002.js
 *  ECMA Section:       15.6.4.9
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */

/*
 * Since regular expressions have been part of JavaScript since 1.2, there
 * are already tests for regular expressions in the js1_2/regexp folder.
 *
 * These new tests try to supplement the existing tests, and verify that
 * our implementation of RegExp conforms to the ECMA specification, but
 * does not try to be as exhaustive as in previous tests.
 *
 * The [,limit] argument to String.split is new, and not covered in any
 * existing tests.
 *
 * String.split cases are covered in ecma/String/15.5.4.8-*.js.
 * String.split where separator is a RegExp are in
 * js1_2/regexp/string_split.js
 *
 */

    var SECTION = "ecma_2/String/split-002.js";
    var VERSION = "ECMA_2";
    var TITLE   = "String.prototype.split( regexp, [,limit] )";

    startTest();

    // the separator is not supplied
    // separator is undefined
    // separator is an empty string

//    AddSplitCases( "splitme", "", "''", ["s", "p", "l", "i", "t", "m", "e"] );
//    AddSplitCases( "splitme", new RegExp(), "new RegExp()", ["s", "p", "l", "i", "t", "m", "e"] );

    // separator is an empty regexp
    // separator is not supplied

    CompareSplit( "hello", "ll" );

    CompareSplit( "hello", "l" );
    CompareSplit( "hello", "x" );
    CompareSplit( "hello", "h" );
    CompareSplit( "hello", "o" );
    CompareSplit( "hello", "hello" );
    CompareSplit( "hello", undefined );

    CompareSplit( "hello", "");
    CompareSplit( "hello", "hellothere" );

    CompareSplit( new String("hello" ) );


    Number.prototype.split = String.prototype.split;

    CompareSplit( new Number(100111122133144155), 1 );
    CompareSplitWithLimit(new Number(100111122133144155), 1, 1 );

    CompareSplitWithLimit(new Number(100111122133144155), 1, 2 );
    CompareSplitWithLimit(new Number(100111122133144155), 1, 0 );
    CompareSplitWithLimit(new Number(100111122133144155), 1, 100 );
    CompareSplitWithLimit(new Number(100111122133144155), 1, void 0 );
    CompareSplitWithLimit(new Number(100111122133144155), 1, Math.pow(2,32)-1 );
    CompareSplitWithLimit(new Number(100111122133144155), 1, "boo" );
    CompareSplitWithLimit(new Number(100111122133144155), 1, -(Math.pow(2,32)-1) );
    CompareSplitWithLimit( "hello", "l", NaN );
    CompareSplitWithLimit( "hello", "l", 0 );
    CompareSplitWithLimit( "hello", "l", 1 );
    CompareSplitWithLimit( "hello", "l", 2 );
    CompareSplitWithLimit( "hello", "l", 3 );
    CompareSplitWithLimit( "hello", "l", 4 );


/*
    CompareSplitWithLimit( "hello", "ll", 0 );
    CompareSplitWithLimit( "hello", "ll", 1 );
    CompareSplitWithLimit( "hello", "ll", 2 );
    CompareSplit( "", " " );
    CompareSplit( "" );
*/

    // separartor is a regexp
    // separator regexp value global setting is set
    // string is an empty string
    // if separator is an empty string, split each by character

    // this is not a String object

    // limit is not a number
    // limit is undefined
    // limit is larger than 2^32-1
    // limit is a negative number

    test();

function CompareSplit( string, separator ) {
    split_1 = string.split( separator );
    split_2 = string_split( string, separator );

    AddTestCase(
        "( " + string +".split(" + separator + ") ).length" ,
        split_2.length,
        split_1.length );

    var limit = split_1.length > split_2.length ?
                    split_1.length : split_2.length;

    for ( var split_item = 0; split_item < limit; split_item++ ) {
        AddTestCase(
            string + ".split(" + separator + ")["+split_item+"]",
            split_2[split_item],
            split_1[split_item] );
    }
}

function CompareSplitWithLimit( string, separator, splitlimit ) {
    split_1 = string.split( separator, splitlimit );
    split_2 = string_split( string, separator, splitlimit );

    AddTestCase(
        "( " + string +".split(" + separator + ", " + splitlimit+") ).length" ,
        split_2.length,
        split_1.length );

    var limit = split_1.length > split_2.length ?
                    split_1.length : split_2.length;

    for ( var split_item = 0; split_item < limit; split_item++ ) {
        AddTestCase(
            string + ".split(" + separator  + ", " + splitlimit+")["+split_item+"]",
            split_2[split_item],
            split_1[split_item] );
    }
}

function string_split ( __this, separator, limit ) {
    var result_1 = String(__this );               // 1

    var A = new Array();                          // 2

    if  ( separator == undefined ) {              // 3
        A[0] = result_1;
        return A;
    }

    if ( limit == undefined ) {                   // 4
        result_4 = Math.pow(2, 31 ) -1;
    } else {
        result_4 = ToUint32( limit );
    }

    var result_5 = result_1.length;                // 5
    var p = 0;                                     // 6

    if ( separator.constructor == RegExp ) {       // 7
        return goto_step_16( p, result_1, A, separator );
    }

    var result_8 = String(separator);              // 8
    var result_9 = result_8.length;                // 9

    while ( true ) {                              // 10, 11, 15
        if ( A.length == result_4 ) {
            return A;
        }

        var found_separator = false;

        for (   k = p;
                ;
                k++ )
        {

            if ( k + result_9 <= p && k + result_9 <= result_5 ) {
                found_separator = true; //  pattern is the empty string
                continue;
            }
            if ( k + result_9 > result_5 ) {
                found_separator = false;
                break;
            }

            for ( j = 0; j < result_9; j++ ) {
                if ( result_1.charAt( k + j ) != result_8.charAt( j ) ) {
                    found_separator = false;
                    break;
                } else {
                    found_separator = true;
                }
            }
            if ( found_separator ) {
                break;
            }
        }

        if ( !found_separator  ) {
            return goto_step_22( p, result_1, A );
        } else {
            result_12 = result_1.substring( p, k );  // 12
            A[A.length] = result_12;                 // 13
        }

        p = k + result_9;                       // 14
    }
    function goto_step_10(p, result_1, A, regexp ) {

    }
    function goto_step_16(p, result_1, A, regexp ) {
        while ( true ) {
            if ( A.length == result_4 ) {                // 16
                return A;
            }

            // 17

            for ( k = p; k < result_1.length; k++ ) {
                match_result =
                    result_1.substring(k, result_1.length).match(regexp);

                if ( match_result == null ) {
                    return goto_step_22( p, result_1, A );
                }
            }

            var m = result_17.length                    // 18
            var result_19 = result_1.substring( p, k );
            A[A.length] = result_19;
        }
    }
    function goto_step_22( p, result_1, A ) {
        result_22 = result_1.substring( p, result_1.length );  // 22
        A[A.length] = result_22;                               // 23
        return A;                                              // 24
    }
}

function ToUint32( n ) {
    n = Number( n );
    var sign = ( n < 0 ) ? -1 : 1;

    if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
        return 0;
    }
    n = sign * Math.floor( Math.abs(n) )

    n = n % Math.pow(2,32);

    if ( n < 0 ){
        n += Math.pow(2,32);
    }

    return ( n );
}
