function testCall() {
    var g = newGlobal({sameCompartmentAs: this});
    for (var i = 0; i < 20; i++) {
        assertEq(objectGlobal(g.Array(1, 2, 3)), g);
        assertEq(objectGlobal(new g.Array(1, 2, 3)), g);
    }
    for (var i = 0; i < 20; i++) {
        g.Error();
        g.assertCorrectRealm();
        if (i > 15)
            g.gc();
    }
}
testCall();

function testAccessor() {
    var g = newGlobal({sameCompartmentAs: this});
    var o = {};
    Object.defineProperty(o, "foo", {get: g.assertCorrectRealm,
                                     set: g.assertCorrectRealm});
    for (var i = 0; i < 20; i++)
        o.foo;
    for (var i = 0; i < 20; i++)
        o.foo = 1;
    Object.defineProperty(o, "arr", {get: g.Array});
    for (var i = 0; i < 20; i++) {
        var arr = o.arr;
        assertEq(objectGlobal(arr), g);
        assertEq(arr.__proto__, g.Array.prototype);
    }
}
testAccessor();
