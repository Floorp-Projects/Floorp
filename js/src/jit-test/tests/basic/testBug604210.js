function f() {
    var msg = '';
    try {
        var x = undefined;
        print(x.foo);
    } catch (e) {
        msg = '' + e;
    }
    assertEq(msg, `TypeError: x is undefined, can't access property "foo" of it`);
}
f();
