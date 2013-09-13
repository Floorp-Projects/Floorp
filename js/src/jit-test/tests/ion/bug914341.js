function f() {
    assertEq(typeof eval("this"), "object");
}
for (var i=0; i<5; i++)
    f();
