/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Igor Bukanov
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 543839;
var summary = 'js_GetMutableScope caller must lock the object';
var actual;
var expect = 1;

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

function test()
{
    jit(true);
    for (var i = 0; i != 100; ++i)
        var tmp = { a: 1 };
    return 1;
}

if (typeof evalcx == 'undefined')
{
    print('Skipping. This test requires evalcx.');
    actual = expect;
} else {
    test();
    test();
    test();
    actual = evalcx("test()", this);
}

reportCompare(expect, actual, summary);
