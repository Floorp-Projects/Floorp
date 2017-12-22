/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Jason Orendorff, Brendan Eich
 */

if (typeof newGlobal == 'function') {
    var gsame = newGlobal('same-compartment');

    gsame.eval("function f() { return this; }");
    f = gsame.f;
    assertEq(f(), gsame);

    gsame.eval("function g() { 'use strict'; return this; }");
    g = gsame.g;
    assertEq(g(), undefined);

    var gnew = newGlobal();

    gnew.eval("function f() { return this; }");
    f = gnew.f;
    assertEq(f(), gnew);

    gnew.eval("function g() { 'use strict'; return this; }");
    g = gnew.g;
    assertEq(g(), undefined);
}

reportCompare(0, 0, "ok");
