var c = 0;
for (var i = 0; i < 5; i++) {
    try {
        Object.defineProperty([], "length", {
            configurable: true,
            enumerable: true,
            writable: true,
            value: 0
        });
    } catch (e) {
        assertEq(e instanceof TypeError, true);
        c++;
    }
}
assertEq(c, i);
