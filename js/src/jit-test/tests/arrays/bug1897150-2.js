var typedArr = Uint8Array.from([1,2,3,4])
var global = 1;

var comparator = function(a, b) {
  assertEq(this.global, 1);
  return b - a;
}

typedArr.sort(comparator);
