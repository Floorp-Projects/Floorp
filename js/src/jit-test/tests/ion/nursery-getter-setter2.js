function g(o, i) {
    o.foo = i;
    assertEq(o.foo, i + 1);
}
function f() {
    var o2 = Object.create({get foo() { return this.x; }, set foo(x) { this.x = x + 1; }});
    for (var i=0; i<1200; i++) {
	g(o2, i);
    }
}
f();
