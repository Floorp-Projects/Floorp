function getType(v) {
    return typeof v;
}
function f() {
    for (var i=0; i<100; i++) {
        assertEq(getType({}), "object");
        assertEq(getType(Math.abs), "function");
        assertEq(getType(10), "number");
        assertEq(getType(Math.PI), "number");
        assertEq(getType(true), "boolean");
        assertEq(getType(""), "string");
        assertEq(getType(null), "object");
        assertEq(getType(undefined), "undefined");
    }
}
f();
