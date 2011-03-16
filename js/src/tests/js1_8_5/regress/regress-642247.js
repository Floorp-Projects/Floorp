/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (typeof timeout == "function") {
    assertEq(typeof timeout(), "number");
    assertEq(typeof timeout(1), "undefined");
}

reportCompare(0, 0, "ok");

