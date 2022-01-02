// A for-of loop over an array does not take a snapshot of the array elements.
// Instead, each time the loop needs an element from the array, it gets its current value.

var a = [3, 5, 5, 4, 0, 5];
var s = '';
for (var i of a) {
    s += i;
    a[i] = 'X';
}
assertEq(s, '355X0X');
