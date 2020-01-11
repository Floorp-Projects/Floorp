function f() {
    var ex;
    try {
        var x = undefined;
        print(x.foo);
    } catch (e) {
        ex = e;
    }
    assertEq(ex.message.endsWith(`x is undefined`), true);
}
f();
