/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'regress-531037.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 531037;
var summary = 'Checking corner cases of eval(source, scope) form';
var actual;
var expect = "No crash";

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test() {
    enterFunc ('test');
    printBugNumber(BUGNUMBER);
    printStatus (summary);

    var b = 10;
    var fff = function() { return --b >= 0; };
    var src = "while (fff());";
    eval(src, null);
    b = 10;
    try {
        eval(src, {fff: function() {throw 0;}});
        throw new Error("Unexpected success of eval");
    } catch (e) {
        if (e !== 0)
            throw e;
    }
    actual = "No crash";
    reportCompare(expect, actual, summary);
    exitFunc ('test');
}

