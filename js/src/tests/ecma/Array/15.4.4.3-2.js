var arr = [0,1,,3,4];
Object.prototype[2] = 2;

assertEq(arr.join(""), "01234");
assertEq(arr.join(","), "0,1,2,3,4");

arr[2] = "porkchops";
assertEq(arr.join("*"), "0*1*porkchops*3*4");

delete Object.prototype[2];
assertEq(arr.join("*"), "0*1*porkchops*3*4");

delete arr[2];
assertEq(arr.join("*"), "0*1**3*4");

Object.prototype[2] = null;
assertEq(arr.join("*"), "0*1**3*4");
Object.prototype[2] = undefined;
assertEq(arr.join("*"), "0*1**3*4");
arr[2] = null;
assertEq(arr.join("*"), "0*1**3*4");
arr[2] = undefined;
assertEq(arr.join("*"), "0*1**3*4");

var arr = new Array(10);
assertEq(arr.join(""), "");
assertEq(arr.join(), ",,,,,,,,,");
assertEq(arr.join("|"), "|||||||||");

arr[2] = "doubt";
assertEq(arr.join(","), ",,doubt,,,,,,,");

arr[9] = "failure";
assertEq(arr.join(","), ",,doubt,,,,,,,failure");

delete arr[2];
assertEq(arr.join(","), ",,,,,,,,,failure");

reportCompare(true, true);
