// |jit-test| skip-if: !this.SharedArrayBuffer || largeArrayBufferEnabled()

// The length exceeds INT32_MAX and should be rejected.

var failed = false;
try {
    var sab = new SharedArrayBuffer((2147483648));
}
catch (e) {
    failed = true;
}
assertEq(failed, true);
