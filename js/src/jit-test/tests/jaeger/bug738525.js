// Test IC for getters backed by a JSNative.
function test1() {
    for (var i = 0; i < 60; i++) {
        assertEq(it.customNative, undefined);
    }

    var res = 0;
    for (var i = 0; i < 60; i++) {
        it.customNative = i;
        res += it.customNative;
    }

    assertEq(res, 1770);
}
function test2() {
    function getValue() {
        return it.customNative;
    }

    for (var i = 0; i < 60; i++) {
        it.customNative = i;
        assertEq(getValue(), i);
    }

    for (var i = 0; i < 60; i++) {
        it.customNative = null;
        assertEq(getValue(), null);

        delete it["customNativ" + "e"];
        assertEq(getValue(), undefined);
        assertEq(it.customNative, undefined);
    }
}
if ("it" in this) {
    test1();
    test2();
}
