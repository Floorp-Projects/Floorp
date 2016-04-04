function f() {
    var o = {x: 1};
    for (var i = 0; i < 300; i++)
        o = Object.create(o);
    for (var i = 0; i < 15; i++) {
        assertEq(o.x, 1);
        assertEq(o.y, undefined);
    }
}
f();
