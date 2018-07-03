function f() {
    for (var i = 0; i < 1200; i++) {
        var o1 = Reflect.construct(Array, [], Object);
        var o2 = Reflect.construct(String, [""], Object);
        var o3 = Reflect.construct(Int32Array, [0], Object);
        assertEq(o1.__proto__, Object.prototype);
        assertEq(o2.__proto__, Object.prototype);
        assertEq(o3.__proto__, Object.prototype);
    }
}
f();
