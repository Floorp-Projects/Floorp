// Array with sparse element (because non-writable).
var arr = [];
Object.defineProperty(arr, 0, {writable: false, configurable: true, enumerable: true, value: 0});

for (var p in arr) {
  // Replace sparse element with dense element.
  assertEq(p, "0");
  delete arr[0];
  arr[0] = 0;
  arr[1] = 1;
  arr.reverse();
}
