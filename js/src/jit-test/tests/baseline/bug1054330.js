function testSplit1() {
    function split(s, sep) {
        return s.split(sep);
    }
    for (var i=0; i<10; i++) {
        var arr = split("a,,b", ",");
        assertEq(arr.length, 3);
        assertEq(arr[0], "a");
        assertEq(arr[1], "");
        assertEq(arr[2], "b");
    }

    // Now pass a different separator to the stub.
    var arr = split("a,,b.c", ".");
    assertEq(arr.length, 2);
    assertEq(arr[0], "a,,b");
    assertEq(arr[1], "c");
}

function testSplit2() {
    function split(s, sep) {
        return s.split(sep);
    }
    for (var i=0; i<10; i++) {
        var arr = split("0101", 0);
        assertEq(arr.length, 3);
        assertEq(arr[0], "");
        assertEq(arr[1], "1");
        assertEq(arr[2], "1");
    }

    // Now pass a different separator to the stub.
    var arr = split("0101", 1);
    assertEq(arr.length, 3);
    assertEq(arr[0], "0");
    assertEq(arr[1], "0");
    assertEq(arr[2], "");
}

function testSplit3() {
    for (var i=0; i<100; i++) {
        var arr = "a|b|c".split("|");
        arr.push("d");
        assertEq(arr.length, 4);
    }
}

testSplit1();
testSplit2();
testSplit3();
