function *f() {
    var o = new Proxy({}, {
        get: function() { assertEq(0, 1); },
        has: function() { assertEq(0, 2); }
    });

    with (o) {
        yield 1;
        with ({}) {
            yield 2;
        }
    }
    with ({".generator": 100}) {
        yield eval("3");
    }
}
var s = "";
for (var i of f())
    s += i;
assertEq(s, "123");
