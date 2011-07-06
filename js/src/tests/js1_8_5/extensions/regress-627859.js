// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var x = 42;
function a() { 
    var x;
    function b() {
        x = 43;
        // When jsparse.cpp's CompExprTransplanter transplants the
        // comprehension expression 'x' into the scope of the 'for' loop,
        // it must not bring the placeholder definition node for the
        // assignment to x above along with it. If it does, x won't appear
        // in b's lexdeps, we'll never find out that the assignment refers
        // to a's x, and we'll generate an assignment to the global x.
        (x for (x in []));
    }
    b();
}
a();
assertEq(x, 42);

reportCompare(true, true);
