// |reftest| require-or(debugMode,skip)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var a = 0;
function f() {
    let (a = let (x = 1) x) {}
}

trap(f, 4, 'assertEq(evalInFrame(1, "a"), 0)');
f();

reportCompare(0, 0, 'ok');
