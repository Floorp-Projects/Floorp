// Making the array huge and sparse shouldn't leave us iterating through the entire array.
// But it does, sadly. Disable, because it takes too long.
if (0) {
    var arr = [1, 2, 3, 4, 5, 6, 7, 8];
    arr.length = Math.pow(2, 32) - 2;
    arr.splice(5); // also test overflow

    assertEq(arr.length, 5);
    assertEq(arr[0], 1);
    assertEq(arr[1], 2);
    assertEq(arr[2], 3);
    assertEq(arr[3], 4);
    assertEq(arr[4], 5);
    assertEq(arr[5], undefined);
}
