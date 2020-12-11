// Ensure certain self-hosted built-in functions are small enough to be inlinable.

assertEq(isSmallFunction(isFinite), true);
assertEq(isSmallFunction(isNaN), true);

assertEq(isSmallFunction(Number.isFinite), true);
assertEq(isSmallFunction(Number.isNaN), true);

assertEq(isSmallFunction(Number.isInteger), true);
assertEq(isSmallFunction(Number.isSafeInteger), true);
