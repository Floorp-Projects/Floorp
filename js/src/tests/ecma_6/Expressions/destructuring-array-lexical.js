var BUGNUMBER = 1184922;
var summary = "Array destructuring with accessing uninitialized lexical binding.";

print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(() => { let y = [y] = []; },
                       ReferenceError);
assertThrowsInstanceOf(() => { let y = [y] = [,]; },
                       ReferenceError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
