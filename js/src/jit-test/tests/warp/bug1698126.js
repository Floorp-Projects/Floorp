function bar() { return arguments[0]; }

function foo() {
    var arr = new Float32Array(10);
    return bar(arr[0]);
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    assertEq(foo(), 0);
}
