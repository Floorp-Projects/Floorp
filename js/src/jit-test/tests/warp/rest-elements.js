// |jit-test| --ion-inlining=off; --fast-warmup
function calleeWithFormals(a, b, ...arr) {
    assertEq(b, 2);
    if (arr.length > 0) {
        assertEq(arr[0], 3);
    }
    if (arr.length > 1) {
        assertEq(arr[1], Math);
    }
    if (arr.length > 2) {
        assertEq(arr[2], "foo");
    }
    return arr;
}
function calleeWithoutFormals(...arr) {
    if (arr.length > 0) {
        assertEq(arr[0], 3);
    }
    if (arr.length > 1) {
        assertEq(arr[1], Math);
    }
    if (arr.length > 2) {
        assertEq(arr[2], "foo");
    }
    return arr;
}
function f() {
    for (var i = 0; i < 100; i++) {
        assertEq(calleeWithFormals(1, 2).length, 0);
        assertEq(calleeWithFormals(1, 2, 3).length, 1);
        assertEq(calleeWithFormals(1, 2, 3, Math).length, 2);
        assertEq(calleeWithFormals(1, 2, 3, Math, "foo").length, 3);

        assertEq(calleeWithoutFormals().length, 0);
        assertEq(calleeWithoutFormals(3).length, 1);
        assertEq(calleeWithoutFormals(3, Math).length, 2);
        assertEq(calleeWithoutFormals(3, Math, "foo").length, 3);
    }
}
f();
