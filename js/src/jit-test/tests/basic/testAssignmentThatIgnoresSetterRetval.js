var o = {
    set x(v) {
        return 42;
    }
};

for (var i = 0; i < 10; ++i) {
    var z = o.x = "choose me";
    assertEq(z, "choose me");
}
