for (var i=0; i<2; i++) {
    var o = {};
    Object.setPrototypeOf(o, null);
    o = Object.create(o);
    var p = {};
    Object.setPrototypeOf(p, o);
}
function f() {
    for (var i=1; i<20; i++)
        p[i] = i;
    for (var i=0; i<1500; i++)
        assertEq(p[0], undefined);
}
f();
