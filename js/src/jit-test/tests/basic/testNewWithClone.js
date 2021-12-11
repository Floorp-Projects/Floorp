with({}) {
    function f() {
        this.foo = "bar";
    }
    o = new f();
    assertEq(o.foo, "bar");
}
