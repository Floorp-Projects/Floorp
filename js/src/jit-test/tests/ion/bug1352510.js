function maybeSetLength(arr, b) {
    if (b) arr.length = 0x7fffffff;
}
var arr = [];
for (var i = 0; i < 2000; i++) {
    maybeSetLength(arr, i > 1500);
    var res = arr.push((0.017453));
}
