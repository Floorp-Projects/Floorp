/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* This shouldn't trigger an assertion. */
(function () {
    eval("var x=delete(x)")
})();

reportCompare(0, 0, "ok");
