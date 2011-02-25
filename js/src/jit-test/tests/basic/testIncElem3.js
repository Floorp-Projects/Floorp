var arr;
for (var j = 0; j < 2 * RUNLOOP; ++j ) {
    arr = [,];
    ++arr[0];
}
assertEq(isNaN(arr[0]), true);
