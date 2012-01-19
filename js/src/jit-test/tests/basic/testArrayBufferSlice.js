function testSlice() {
    function test(subBuf, starts, size) {
        var byteLength = size;
        var subBuffer = eval(subBuf);
        var subArray = new Int8Array(subBuffer);
        assertEq(subBuffer.byteLength, byteLength);
        for (var i = 0; i < size; ++i)
            assertEq(starts + i, subArray[i]);
    }

    var buffer = new ArrayBuffer(32);
    var array = new Int8Array(buffer);
    for (var i = 0; i < 32; ++i)
        array[i] = i;

    test("buffer.slice(0)", 0, 32);
    test("buffer.slice(16)", 16, 16);
    test("buffer.slice(24)", 24, 8);
    test("buffer.slice(32)", 32, 0);
    test("buffer.slice(40)", 32, 0);
    test("buffer.slice(80)", 32, 0);

    test("buffer.slice(-8)", 24, 8);
    test("buffer.slice(-16)", 16, 16);
    test("buffer.slice(-24)", 8, 24);
    test("buffer.slice(-32)", 0, 32);
    test("buffer.slice(-40)", 0, 32);
    test("buffer.slice(-80)", 0, 32);

    test("buffer.slice(0, 32)", 0, 32);
    test("buffer.slice(0, 16)", 0, 16);
    test("buffer.slice(8, 24)", 8, 16);
    test("buffer.slice(16, 32)", 16, 16);
    test("buffer.slice(24, 16)", 24, 0);

    test("buffer.slice(16, -8)", 16, 8);
    test("buffer.slice(-20, 30)", 12, 18);

    test("buffer.slice(-8, -20)", 24, 0);
    test("buffer.slice(-20, -8)", 12, 12);
    test("buffer.slice(-40, 16)", 0, 16);
    test("buffer.slice(-40, 40)", 0, 32);
}

testSlice();
