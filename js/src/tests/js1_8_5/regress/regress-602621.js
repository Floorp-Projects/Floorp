/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function test(arg) {
    eval(arg);
    {
        function arguments() { return 1; }
    }
    return arguments;
}

reportCompare("function", typeof test("42"), "function sub-statement must override arguments");
