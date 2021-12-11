function maybeFreeze(arr, b) {
    with(this) {}; // Don't inline this.
    if (b) {
        Object.freeze(arr);
    }
}
function test() {
    var arr = [];
    for (var i = 0; i < 1800; i++) {
        maybeFreeze(arr, i > 1500);
        try {
            arr.push(2);
            assertEq(i <= 1500, true);
        } catch(e) {
            assertEq(e instanceof TypeError, true);
        }
    }
}
test();
