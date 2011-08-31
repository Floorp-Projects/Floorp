// arr is an Int8Array, then changes to Int16Array.
// This should trigger recompilation of f.
var arr = new Int8Array(100);
var arr16 = new Int16Array(100);
arr16[2] = 12345;
function f(a) {
    var x;
    for(var i=0; i<30; i++) {
        x = a[2];
    }
    return x;
}
assertEq(f(arr), 0);
assertEq(f(arr), 0);
this.arr = arr16;
assertEq(f(arr), 12345);
