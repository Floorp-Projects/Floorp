function testNumBoundArgsGuard() {
    function f(a, b, c, d, e, f, g, h) {
        return [a, b, c, d, e, f, g, h].join(",");
    }
    var arr = [
        f.bind(null, 1, 2, 3, 4, 5, 6),
        f.bind(null, 4, 5, 6, 7, 8, 9),
        f.bind(null, 1, 2),
        f.bind(null, 1),
    ];
    var expected = [
        "1,2,3,4,5,6,10,11",
        "4,5,6,7,8,9,10,11",
        "1,2,10,11,12,13,14,15",
        "1,10,11,12,13,14,15,",
    ];
    for (var i = 0; i < 100; i++) {
        assertEq(arr[0](10, 11, 12, 13, 14, 15), expected[0]);
        assertEq(arr[1](10, 11, 12, 13, 14, 15), expected[1]);
        assertEq(arr[2](10, 11, 12, 13, 14, 15), expected[2]);
        assertEq(arr[3](10, 11, 12, 13, 14, 15), expected[3]);
        assertEq(arr[i % arr.length](10, 11, 12, 13, 14, 15), expected[i % arr.length]);
    }
}
testNumBoundArgsGuard();

function testJSFunctionGuard() {
    var arr = [
        (x => x * 0).bind(null, 1),
        (x => x * 1).bind(null, 2),
        (x => x * 2).bind(null, 3),
        (x => x * 3).bind(null, 4),
        (x => x * 4).bind(null, 5),
        (x => x * 5).bind(null, 6),
        (x => x * 6).bind(null, 7),
        (x => x * 7).bind(null, 8),
        (x => x * 8).bind(null, 9),
    ];
    var boundNative = Math.abs.bind(null, -123);
    var boundProxy = (new Proxy(x => x * 42, {})).bind(null, 7);

    for (var i = 0; i < 100; i++) {
        var idx = i % arr.length;
        var fun1 = arr[idx];
        var expected1 = idx * (idx + 1);
        assertEq(fun1(), expected1);
        var fun2 = i > 70 ? boundNative : fun1;
        assertEq(fun2(), i > 70 ? 123 : expected1);
        var fun3 = i > 70 ? boundProxy : fun1;
        assertEq(fun3(), i > 70 ? 294 : expected1);
    }
}
testJSFunctionGuard();

function testCrossRealmTarget() {
    globalThis.num = "FAIL";

    var globals = [];
    for (var i = 0; i < 4; i++) {
        var g = newGlobal({sameCompartmentAs: this});
        g.num = i;
        globals.push(g);
    }
    var arr = globals.map(g => g.evaluate("(x => this.num)"));
    arr = arr.map(f => Function.prototype.bind.call(f, null));

    for (var i = 0; i < 200; i++) {
        assertEq(arr[0](), 0);
        var idx = i % arr.length;
        assertEq(arr[idx](), idx);
    }
}
testCrossRealmTarget();

// Bug 1819651.
function testReturnsItself() {
    var fun = function() { return boundFun; };
    var boundFun = fun.bind(null);
    for (var i = 0; i < 20; i++) {
        assertEq(boundFun(), boundFun);
        assertEq(new boundFun(), boundFun);
    }
}
testReturnsItself();
