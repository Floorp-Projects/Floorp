var b = 1.5;
var arr;
function f_int(arr, index) {
    for (var i=0; i<100; i++) {
        arr[index]++;
    }
}
arr = [1, 2, 3];
f_int(arr, "1");
assertEq(arr[1], 102);
arr = [1, 2, 3];
f_int(arr, 1);
assertEq(arr[1], 102);

function f_double(arr, index) {
    for (var i=0; i<100; i++) {
        arr[+Math.pow(index,1.0)*1.5/b]++;
    }
}
arr = [1, 2, 3];
f_double(arr, 1.0);
assertEq(arr[1], 102);
arr = [1, 2, 3];
f_double(arr, NaN);
assertEq(arr[1], 2);
