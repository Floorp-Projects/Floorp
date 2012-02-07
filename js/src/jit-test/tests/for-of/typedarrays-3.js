// Destructuring does not occur when the target of for-of is an empty typed array.

for (var [[x]] of Int32Array(0))
    throw "FAIL";
