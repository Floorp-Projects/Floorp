function f(depth) {
    function Obj() {
        this.prop = null;
        this.prop = this;
    }
    var o = new Obj();
    assertEq(o.prop, o);
    if (depth < 1000) {
        f(depth + 1);
    }
}
f(0);
