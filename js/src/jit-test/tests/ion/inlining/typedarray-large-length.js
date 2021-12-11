var SINGLETON_BYTE_LENGTH = 1024 * 1024 * 10;

for (var i = 0; i < 2000; i++) {
    if (i == 1500)
        len = SINGLETON_BYTE_LENGTH >> 3;
    else
        len = i % 64;
    var arr = new Float64Array(len);
    assertEq(arr[0], len ? 0 : undefined);
}
