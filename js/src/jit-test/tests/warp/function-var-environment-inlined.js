function defaultValue() { return 3; }

function testCallee(p = defaultValue()) {
    var q = p + 1;
    return () => q + p;
}
function test() {
    var res = 0;
    for (var i = 0; i < 2000; i++) {
        res += testCallee()();
        res += testCallee(1)();
    }
    assertEq(res, 20000);
}
test();
