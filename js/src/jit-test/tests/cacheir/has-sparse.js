function sparse() {
    var o = {0: 0, 0x10000: 0};

    var tests = [[1, false], [0, true], [-2, false], [0x10000, true], [0x20000, false]];
    for (var [key, has] of tests) {
        assertEq(key in o, has);
        assertEq(o.hasOwnProperty(key), has);
    }
}

function typedArray() {
    var o = {0: 0, 0x10000: 0};
    var t = new Int32Array(0x10001)

    // Only use Int32Array after we attached the sparse stub
    //                in o,  in t
    var tests = [[1, [false, true]],
                 [0, [true, true]],
                 [-2, [false, false]],
                 [0x10000, [true, true]],
                 [0x20000, [false, false]]];

    for (var i = 0; i < 10; i++) {
        for (var [key, has] of tests) {
            assertEq(key in o, has[i > 5 ? 1 : 0]);
            assertEq(o.hasOwnProperty(key), has[i > 5 ? 1 : 0]);
        }

        if (i == 5)
            o = t;
    }
}

function protoChange() {
    var o = {0: 0, 0x10000: 0};

    var tests = [[1, [false, true]],
                 [0, [true, true]],
                 [-2, [false, false]],
                 [0x10000, [true, true]],
                 [0x20000, [false, false]]];

    for (var i = 0; i < 10; i++) {
        for (var [key, has] of tests) {
            assertEq(key in o, has[i > 5 ? 1 : 0]);
            // Proto change doesn't affect hasOwnProperty.
            assertEq(o.hasOwnProperty(key), has[0]);
        }

        if (i == 5)
            o.__proto__ = [1, 1, 1, 1];
    }
}

sparse();
typedArray();
protoChange();

