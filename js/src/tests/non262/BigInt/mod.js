// Any copyright is dedicated to the Public Domain.
// https://creativecommons.org/licenses/publicdomain/

// Check that |x % x| returns zero when |x| contains multiple digits
assertEq(0x10000000000000000n % 0x10000000000000000n, 0n);
assertEq(-0x10000000000000000n % -0x10000000000000000n, 0n);

reportCompare(true, true);
