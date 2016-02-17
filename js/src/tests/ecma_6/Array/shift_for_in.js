var BUGNUMBER = 1247701;
var summary = 'Array.prototype.shift on a dense array with holes should update for-in enumeration properties.';

print(BUGNUMBER + ": " + summary);

var x = ["a", , "b", , "c", "d" , "e", "f", "g"];
for (var p in x) {
  assertEq(p in x, true);
  x.shift();
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
