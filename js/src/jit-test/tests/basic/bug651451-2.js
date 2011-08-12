var arr = [1, 2, 3, 4, 5];
arr.length = 100;
arr.pop();
assertEq(arr.length, 99);
arr.pop();
assertEq(arr.length, 98);