function *f() {
    var o = Proxy.createFunction({
        get: function() { assertEq(0, 1); },
        has: function() { assertEq(0, 2); }
    }, function() {});

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
