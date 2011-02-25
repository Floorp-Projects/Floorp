/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function f(x) {
    delete arguments[0];
    for(var i=0; i<20; i++) {
        arguments[0] !== undefined;
    }
}

/* Don't crash. */
f(1);

reportCompare(0, 0, "ok");
