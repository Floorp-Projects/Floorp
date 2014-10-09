function f() {
    assertEq(typeof this, "object");
}
this.f();
function gg() {
    for (var j = 0; j < 3; ++j) {
        f();
    }
};
gg();
