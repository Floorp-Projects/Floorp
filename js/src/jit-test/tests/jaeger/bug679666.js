var arr = new Int32Array(20);

function f(a1, a2) {
    for (var i=0; i<10; i++) {
        g1 = a2;
        arr[a1] = a2;
        assertEq(g1, a2);

        if ([1].length === 10) {
            a1 = {};
        }
    }
}

f(1, eval("{}"));

for (var i=0; i<5; i++) {
    f(2, 3);
    f(5, -6.1);
}
assertEq(arr[2], 3);
assertEq(arr[5], -6);
