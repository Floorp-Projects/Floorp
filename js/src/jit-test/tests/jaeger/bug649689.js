function f(x) {
    eval("a = 3");
    x.p = x.p = a;
    assertEq(x.p, 3);
}
f({p: 2});
