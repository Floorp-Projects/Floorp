function f(x) {
    // Check that we do not fold +1 and -2 across truncated/non-truncated operations.
    return (((x | 0) + 1) | 0) + -2;
}
const int32_min = -Math.pow(2,31);
f(Infinity);
assertEq(f(int32_min - 1), int32_min - 2);
