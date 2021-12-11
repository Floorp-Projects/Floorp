var BUGNUMBER = 1384299;
var summary = "yield outside of generators should provide better error";

print(BUGNUMBER + ": " + summary);

let caught = false;
try {
    eval("yield 10");
} catch(e) {
    assertEq(e.message, "yield expression is only valid in generators");
    caught = true;
}
assertEq(caught, true);

try {
    eval("function f() { yield 10; }");
} catch(e) {
    assertEq(e.message, "yield expression is only valid in generators");
    caught = true;
}
assertEq(caught, true);

try {
    eval("async function f() { yield 10; }");
} catch(e) {
    assertEq(e.message, "yield expression is only valid in generators");
    caught = true;
}
assertEq(caught, true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
