/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

try {
    for (var [e] = /x/ in d) {
        function () {}
    }
} catch (e) {}
try {
    let(x = Object.freeze(this, /x/))
    e = #0= * .toString
    function y() {}
} catch (e) {}

reportCompare(0, 0, "don't crash");
