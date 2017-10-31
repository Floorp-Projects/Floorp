class C {};
C.prototype.a = "a";
C.prototype.q = "q";
C.prototype.NaN = NaN;
class D extends C {
    foo(p) {
        return super[p];
    }
}
function f() {
    var d = new D();
    for (let p in C.prototype) {
        assertEq(p, String(d.foo(p)));
    }
}
f();
f();
