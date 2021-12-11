var arr = [2];
arr.pop();
arr[0] = 2;
assertEq(arr.length, 1);
