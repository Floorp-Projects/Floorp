Array.prototype[0] = 'x';
Array.prototype[1] = 'x';
var arr = [];
for (var p in arr) {
    arr[0] = 0;
    arr[1] = 1;
    arr.reverse();
}
