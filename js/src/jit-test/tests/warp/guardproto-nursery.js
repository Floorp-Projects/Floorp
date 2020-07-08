function f() {
    var o = {x: 1, y: 3};
    o.__proto__ = {x: 2};
    var p = Math;
    p.__proto__ = o;
    p.__proto__ = {__proto__: o};

    for (var i = 0; i < 3000; i++) {
        assertEq(p.x, 1);
        assertEq(p.y, 3);
    }
}
f();
