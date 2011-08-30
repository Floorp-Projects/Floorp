function foo() {
};
function f() {
    var e = foo;
    a = new e();
    assertEq(typeof(a), "object");
    e=a;
}
f();
