/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

"use strict";
try {
    eval("(function() { eval(); function eval() {} })");
    assertEq(0, 1);
} catch (e) {
    assertEq(e.name, "SyntaxError");
}

reportCompare(0, 0, "ok");
