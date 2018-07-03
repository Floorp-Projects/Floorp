function testSelfHosted() {
    var g = newGlobal({sameCompartmentAs: this});
    var arr = g.evaluate("[1, 2]");
    for (var i = 0; i < 20; i++) {
        var mapped = Array.prototype.map.call(arr, x => x + 1);
        assertEq(mapped.constructor, Array);
    }
}
testSelfHosted();

function testNative() {
    var g = newGlobal({sameCompartmentAs: this});
    var arr = g.evaluate("[1, 2, 3, 4]");
    for (var i = 0; i < 20; i++) {
        var slice = Array.prototype.slice.call(arr, 0, 3);
        assertEq(slice.constructor, Array);
    }
}
testNative();

function testIntrinsic() {
    var g1 = newGlobal({sameCompartmentAs: this});
    var g2 = newGlobal();
    var IsCrossRealmArrayConstructor = getSelfHostedValue("IsCrossRealmArrayConstructor");
    for (var i = 0; i < 20; i++) {
        assertEq(IsCrossRealmArrayConstructor(Array), false);
        assertEq(IsCrossRealmArrayConstructor(g1.Array), true);
        assertEq(IsCrossRealmArrayConstructor(g2.Array), true);
    }
}
testIntrinsic();
