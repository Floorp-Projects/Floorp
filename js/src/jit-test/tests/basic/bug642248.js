function test(makeNonArray) {
    function C() {}
    C.prototype = []
    c = new C();
    c.push("foo");
    return c.length
}
assertEq(test(true), 1);
var a = [];
var b = Object.create(a);
