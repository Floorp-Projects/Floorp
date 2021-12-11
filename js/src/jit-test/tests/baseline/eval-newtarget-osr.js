function f(expected) {
    eval("for (var i = 0; i < 30; i++) assertEq(new.target, expected)");
}
new f(f);
f(undefined);
