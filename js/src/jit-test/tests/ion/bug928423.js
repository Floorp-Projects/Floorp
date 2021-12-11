function f(o, p) {
    try {} catch(e) {};
    return o[p];
}
function test() {
    var o = {foo: 1, bar: 2, foobar: 3};

    for (var i = 0; i < 30; i++) {
	assertEq(f(o, "foo1".substr(0, 3)), 1);
	assertEq(f(o, "bar1".substr(0, 3)), 2);
	assertEq(f(o, "foobar1".substr(0, 6)), 3);
    }
}
test();
