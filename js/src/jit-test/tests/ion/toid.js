function f(arr, index) {
    for (var i=0; i<100; i++) {
        arr[index]++;
        ++arr[index];
    }
}
var arr = [1, 2, 3];
f(arr, "1");
f(arr, 1);
assertEq(arr[1], 402);
