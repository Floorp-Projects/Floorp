var arr = Float32Array(1);
arr[0] = 15;
var a = arr[0];
assertEq(a, 15);
switch (a) {
  case 15: break;
  default: throw "FAIL";
}

