function f() {
    var ex;
    try {
        var x = undefined;
        print(x.foo);
    } catch (e) {
        ex = e;
    }
    assertEq(/(x is|"foo" of) undefined/.test(ex.message), true);
}
f();
