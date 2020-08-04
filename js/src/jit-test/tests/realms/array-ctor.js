function testArrayRealm() {
    var g = newGlobal();
    var A = g.Array;
    for (var i = 0; i < 100; i++) {
        var a;
        a = new A();
        assertEq(isSameCompartment(a, g), true);
        assertEq(Object.getPrototypeOf(a), A.prototype);

        a = new A(i);
        assertEq(isSameCompartment(a, g), true);
        assertEq(Object.getPrototypeOf(a), A.prototype);
    }
}
testArrayRealm();

function testErrorRealm() {
    var g = newGlobal();
    var A = g.Array;
    for (var i = 50; i > -50; i--) {
        var a = null;
        var ex = null;
        try {
            a = new A(i);
        } catch (e) {
            ex = e;
        }
        if (i >= 0) {
            assertEq(Object.getPrototypeOf(a), A.prototype);
        } else {
            assertEq(ex instanceof g.RangeError, true);
        }
    }
}
testErrorRealm();
