// |jit-test| --ion-limit-script-size=off
function f() {
    var b1 = new ArrayBuffer(64);
    for (var i = 0; i < 100; ++i) {
        Object.defineProperty(b1, "x", {
            configurable: true,
            enumerable: true,
            writable: true,
            value: i
        });
        assertEq(b1.x, i);
    }
}
f();
