function f() {
    var input = [0, 1, 2, 0x80000000, 0xfffffff0,
                 0xffffffff, 0xffffffff + 1, 0xffffffff + 2,
                 101];

    var arr = new Uint32Array(input.length);
    for (var i=0; i < arr.length; i++) {
        arr[i] = input[i];
    }

    var expected = [0, 1, 2, 0x80000000, 0xfffffff0,
                    0xffffffff, 0, 1, 101];
    for (var i=0; i < arr.length; i++) {
        assertEq(arr[i], expected[i]);
    }
}
for (var i=0; i<5; i++) {
    f();
}
