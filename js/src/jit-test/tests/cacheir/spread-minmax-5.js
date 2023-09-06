function f() {
    var arr = [[]];
    for (var i = 1; i < 101; i++) {
        arr[0].push(i);
    }
    var res = 0;
    for (var i = 0; i < 100; i++) {
        res += Math.max(...arr[0]) + Math.min(...arr[0]);
    }
    assertEq(res, 10100);
}
f();
