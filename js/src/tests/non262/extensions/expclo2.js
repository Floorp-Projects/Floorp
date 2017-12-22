// |reftest| skip-if(!release_or_beta)
/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Brendan Eich
 */

var summary = "Partial flat expression closure upvar order test";

function f(a) {
    if (a) {
        let b = 42;
        let c = function () b+a;
        ++b;
        return c;
    }
    return null;
}

var expect = 44;
var actual = f(1)();

reportCompare(expect, actual, summary);
