function f() {
    for (var i = 0; i < 600; i++) {
        var arr = [1.1, ([...[]], 2.2), i];
        for (var j = 0; j < 2; j++) {
            var idx = j === 1 ? 2 : 0;
            var exp = j === 1 ? i + 1 : 2.1;
            assertEq(arr[idx] + 1, exp);
        }
    }
}
f();
