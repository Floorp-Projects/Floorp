/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var arguments;

function b(foo) {
    delete foo.d
    delete foo.w
    foo.d = true
    foo.w = Object
    delete Object.defineProperty(foo, "d", ({
        set: Math.w
    })); {}
}
for each(e in [arguments, arguments]) {
    try {
        b(e)('')
    } catch (e) {}
}

reportCompare(0, 0, "ok");
