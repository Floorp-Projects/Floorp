// Tests by André Bargull <andrebargull@googlemail.com>

// Annex B.3.3.1
function f1() {
    { function* g() {} }
    assertEq(typeof g, "undefined");
}
f1();

// Annex B.3.3.2
{ function* g() {} }
assertEq(typeof g, "undefined");

// Annex B.3.3.3
function f2() {
    eval("{ function* g() {} }");
    assertEq(typeof g, "undefined");
}
f2();

// Annex B.3.3.3
eval("{ function* g() {} }");
assertEq(typeof g, "undefined");

if (typeof reportCompare === "function")
    reportCompare(true, true);
