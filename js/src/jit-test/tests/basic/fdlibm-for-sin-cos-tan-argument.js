// |jit-test| --use-fdlibm-for-sin-cos-tan
// Test that fdlibm is being used for sin, cos, and tan.

// Tests adapted from https://github.com/arkenfox/TZP/blob/master/tests/math.html#L158-L319
assertEq(Math.cos(1e284), 0.7086865671674247);
assertEq(Math.sin(7*Math.LOG10E), 0.10135692924965616);
assertEq(Math.tan(6*Math.E), 0.6866761546452431);
