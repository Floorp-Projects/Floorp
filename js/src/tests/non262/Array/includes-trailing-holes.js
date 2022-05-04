// Array with trailing hole as explicit "magic elements hole".
assertEq([,].includes(), true);
assertEq([,].includes(undefined), true);
assertEq([,].includes(undefined, 0), true);
assertEq([,].includes(null), false);
assertEq([,].includes(null, 0), false);

// Array with trailing hole with no explicit "magic elements hole".
assertEq(Array(1).includes(), true);
assertEq(Array(1).includes(undefined), true);
assertEq(Array(1).includes(undefined, 0), true);
assertEq(Array(1).includes(null), false);
assertEq(Array(1).includes(null, 0), false);

if (typeof reportCompare === "function")
  reportCompare(true, true);
