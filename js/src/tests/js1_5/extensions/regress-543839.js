/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Igor Bukanov
 */

var gTestfile = 'regress-543839.js';
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

test();
test();
test();
actual = evalcx("test()", this);

reportCompare(expect, actual, summary);
