function testBasic() {
    var o1 = {x: 1};
    var o2 = {a: 1, b: 2, c: 3, x: 4, y: 5, z: 6};
    for (var i = 0; i < 4; i++) {
        var to1 = Object.assign({}, o1);
        assertEq(JSON.stringify(to1), '{"x":1}');
        var to2 = Object.assign({}, o2);
        assertEq(JSON.stringify(to2), '{"a":1,"b":2,"c":3,"x":4,"y":5,"z":6}');
    }
}
testBasic();

// Target object's proto must not be changed by assign.
function testProto() {
    var from = {};
    from.a = 1;
    from.b = 2;
    from.c = 3;
    for (var i = 0; i < 5; i++) {
        var to = Object.assign(Object.create(null), from);
        assertEq(JSON.stringify(to), '{"a":1,"b":2,"c":3}');
        assertEq(Object.getPrototypeOf(to), null);
    }
}
testProto();

// Target object's realm must not be changed by assign.
function testRealm() {
    var global = newGlobal({sameCompartmentAs: this});
    var from = global.evaluate("({})");
    from.a = 1;
    from.b = 2;
    from.c = 3;
    for (var i = 0; i < 5; i++) {
        var to = Object.assign({}, from);
        assertEq(JSON.stringify(to), '{"a":1,"b":2,"c":3}');
        assertEq(objectGlobal(to), this);
        assertEq(Object.getPrototypeOf(to), Object.prototype);
    }
}
testRealm();
