setJitCompilerOption("ic.force-megamorphic", 1);

function objectWithHops(hops) {
    var o = Object.create(null);
    o.hops = hops;
    for (var i = 0; i < hops; i++) {
        o = Object.create(o);
    }
    return o;
}
function test() {
    var objs = [];
    for (var i = 0; i < 32; i++) {
        objs.push(objectWithHops(230 + i));
    }
    for (var i = 0; i < 130; i++) {
        var o = objs[i % objs.length];
        assertEq(o.hops, 230 + (i % objs.length));
        assertEq("hops" in o, true);
        assertEq(Object.hasOwnProperty.call(o, "hops"), false);
        assertEq(o.missing, undefined);
        assertEq("missing" in o, false);
        assertEq(Object.hasOwnProperty.call(o, "missing"), false);
    }
}
for (var i = 0; i < 10; i++) {
  test();
}
