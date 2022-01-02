function f() {
    var objs = [];
    for (var i = 0; i < 100; i++) {
        objs[i] = {};
    }
    var o = objs[0];
    var a = new Float64Array(1024);
    function g(a, b) {
        let p = b;
        for (; p.x < 0; p = p.x) {
            while (p === p) {}
        }
        for (var i = 0; i < 10000; ++i) {}
    }
    g(a, o);
}
f();
