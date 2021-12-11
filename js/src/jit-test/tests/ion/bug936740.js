function ceil(x) {
    return Math.ceil(x);
}

// Compiled as Ceil(double -> int32)
assertEq(ceil(1.1), 2);
assertEq(ceil(-1.1), -1);
assertEq(ceil(-3), -3);

// As we use the identity Math.ceil(x) == -Math.floor(-x) and Floor(-0) bails out,
// this should bail out.
assertEq(ceil(0), 0);
assertEq(ceil(0), 0);

// Reuses the Ceil(double -> int32) path
assertEq(ceil(1.1), 2);
assertEq(ceil(-1.1), -1);
assertEq(ceil(-3), -3);

// Bails out and then compiles as Ceil(double -> double)
assertEq(ceil(-0), -0);
assertEq(ceil(Math.pow(2, 32)), Math.pow(2, 32));
assertEq(ceil(-0), -0);

// Still works but not inlined as double -> int32 (it still uses double -> double)
assertEq(ceil(1.5), 2);
