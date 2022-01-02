function testBug502914() {
    // Assigning a non-function to a function-valued property on trace should
    // bump the shape.
    function f1() {}
    function C() {}
    var x = C.prototype = {m: f1};
    x.m();  // brand scope
    var arr = [new C, new C, new C, x];
    try {
        for (var i = 0; i < 4; i++) {
            arr[i].m = 12;
            x.m();  // should throw last time through
        }
    } catch (exc) {
        return exc.constructor.name;
    }
    return "no exception";
}
assertEq(testBug502914(), "TypeError");
