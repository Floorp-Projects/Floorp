function f(arr, len) {
    for (var i = 0; i < 2000; i++) {
        assertEq(arr.length, len);
    }
}
var arr = [0];
f(arr, 1);

arr.length = 0xffff_ffff;
f(arr, 0xffff_ffff);
