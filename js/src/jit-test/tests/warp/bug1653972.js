function maybeSetLength(arr, b) {
    if (b) {
        arr.length = 0x8000_1111;
    }
}
var arr = [];
for (var i = 0; i < 1600; i++) {
    maybeSetLength(arr, i > 1500);
    arr.push(2);
}
assertEq(arr.length, 0x8000_1112);
