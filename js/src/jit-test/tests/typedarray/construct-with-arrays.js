// Test TypedArray constructor when called with iterables or typed arrays.

function testPackedArray() {
    function test() {
        var array = [
            1, 2, 3,
            4, 5, 6,
            7, 8, 9,
        ];
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(array);
            assertEq(ta.length, array.length);
            for (var j = 0; j < array.length; ++j) {
                assertEq(ta[j], array[j]);
            }
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testPackedArray();

function testHoleArray() {
    function test() {
        var array = [
            1, /* hole */, 3,
            4, /* hole */, 6,
            7, /* hole */, 9,
        ];
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(array);
            assertEq(ta.length, array.length);
            for (var j = 0; j < array.length; ++j) {
                assertEq(ta[j], array[j] || 0);
            }
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testHoleArray();

function testTypedArraySameType() {
    function test() {
        var array = new Int32Array([
            1, 2, 3,
            4, 5, 6,
            7, 8, 9,
        ]);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(array);
            assertEq(ta.length, array.length);
            for (var j = 0; j < array.length; ++j) {
                assertEq(ta[j], array[j]);
            }
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testTypedArraySameType();

function testTypedArrayDifferentType() {
    function test() {
        var array = new Float32Array([
            1, 2, 3,
            4, 5, 6,
            7, 8, 9,
        ]);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(array);
            assertEq(ta.length, array.length);
            for (var j = 0; j < array.length; ++j) {
                assertEq(ta[j], array[j]);
            }
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testTypedArrayDifferentType();

function testIterable() {
    function test() {
        var array = [
            1, 2, 3,
            4, 5, 6,
            7, 8, 9,
        ];
        array = Object.defineProperties({
            [Symbol.iterator]() {
                var index = 0;
                return {
                    next() {
                        var done = index >= array.length;
                        var value = !done ? array[index++] : undefined;
                        return {done, value};
                    }
                };
            }
        }, Object.getOwnPropertyDescriptors(array));

        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(array);
            assertEq(ta.length, array.length);
            for (var j = 0; j < array.length; ++j) {
                assertEq(ta[j], array[j]);
            }
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testIterable();

function testWrappedArray() {
    var g = newGlobal();

    function test() {
        var array = new g.Array(
            1, 2, 3,
            4, 5, 6,
            7, 8, 9,
        );
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(array);
            assertEq(ta.length, array.length);
            for (var j = 0; j < array.length; ++j) {
                assertEq(ta[j], array[j]);
            }
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testWrappedArray();

function testWrappedTypedArray() {
    var g = newGlobal();

    function test() {
        var array = new g.Int32Array([
            1, 2, 3,
            4, 5, 6,
            7, 8, 9,
        ]);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(array);
            assertEq(ta.length, array.length);
            for (var j = 0; j < array.length; ++j) {
                assertEq(ta[j], array[j]);
            }
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testWrappedTypedArray();
