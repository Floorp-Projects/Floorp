var arr;
for (var j = 0; j < 18; ++j ) {
    arr = [,];
    ++arr[0];
}
assertEq(isNaN(arr[0]), true);
