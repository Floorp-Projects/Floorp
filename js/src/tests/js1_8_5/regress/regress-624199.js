/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
function roundTrip(f) {
    try {
        eval(uneval(f));
        return true;
    } catch (e) {
        return '' + e;
    }
}

function f() { if (true) { 'use strict'; } var eval; }
assertEq(roundTrip(f), true);

reportCompare(true,true);
