/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* This shouldn't trigger an assertion. */
(function () {
    eval("var x=delete(x)")
})();

reportCompare(0, 0, "ok");
