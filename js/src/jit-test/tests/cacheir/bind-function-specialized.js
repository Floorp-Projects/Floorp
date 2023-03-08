load(libdir + "asserts.js");

function testBasic() {
    var g = function foo(a, b, c) { return a - b - c; };
    for (var i = 0; i < 100; i++) {
        var bound1 = g.bind(null, 1);
        assertEq(bound1.length, 2);
        assertEq(bound1.name, "bound foo");
        var bound2 = bound1.bind(null, 2);
        assertEq(bound2.length, 1);
        assertEq(bound2.name, "bound bound foo");
        assertEq(bound2(9), -10);
    }
}
testBasic();

function testBindNonCtor() {
    var g = (a, b, c) => a - b - c;
    for (var i = 0; i < 100; i++) {
        var bound1 = g.bind(null, 1);
        var bound2 = bound1.bind(null, 2);
        assertEq(bound1(2, 3), -4);
        assertEq(bound2(4), -5);
        assertThrowsInstanceOf(() => new bound1(2, 3), TypeError);
        assertThrowsInstanceOf(() => new bound2(4), TypeError);
        assertEq(bound2.length, 1);
        assertEq(bound2.name, "bound bound g");
    }
}
testBindNonCtor();

function testBindSelfHosted() {
    var g = Array.prototype.map;
    var arr = [1, 2, 3];
    for (var i = 0; i < 100; i++) {
        var bound1 = g.bind(arr);
        var bound2 = bound1.bind(null, x => x + 5);
        assertEq(bound1(x => x + 3).toString(), "4,5,6");
        assertEq(bound2().toString(), "6,7,8");
        assertEq(bound2.length, 0);
        assertEq(bound2.name, "bound bound map");
    }
}
testBindSelfHosted();

function testBoundDeletedName() {
    var g = function foo(a, b, c) { return a - b - c; };
    var bound1 = g.bind(null);
    var bound2 = g.bind(null);
    delete bound2.name;
    for (var i = 0; i < 100; i++) {
        var obj = i < 50 ? bound1 : bound2;
        var bound3 = obj.bind(null);
        assertEq(bound3.length, 3);
        assertEq(bound3.name, i < 50 ? "bound bound foo" : "bound ");
    }
}
testBoundDeletedName();

function testWeirdProto() {
    var g = function foo() { return 123; };
    var proto = {bind: Function.prototype.bind};
    Object.setPrototypeOf(g, proto);
    for (var i = 0; i < 100; i++) {
        var bound1 = g.bind(null);
        assertEq(Object.getPrototypeOf(bound1), proto);
        var bound2 = bound1.bind(null);
        assertEq(Object.getPrototypeOf(bound2), proto);
        assertEq(bound2(), 123);
    }
}
testWeirdProto();
