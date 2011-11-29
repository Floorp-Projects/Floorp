var a = [1, , 2];
Array.prototype[1] = 3;

function f(arr) {
    return arr[0] + arr[1] + arr[2];
}
for (var i=0; i<70; i++) {
    assertEq(f(a), 6);
}

Object.defineProperty(Array.prototype, "1", {
    get: function() {
        return 1234;
    }
});

assertEq(f(a), 1237);
assertEq(f(a), 1237);
