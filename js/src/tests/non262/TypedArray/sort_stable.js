// Test with different lengths to cover the case when InsertionSort is resp.
// is not called.
for (let i = 2; i <= 10; ++i) {
    let length = 2 ** i;
    let ta = new Int8Array(length);

    ta[0] = 2;
    ta[1] = 1;
    ta[2] = 0;

    for (let i = 3; i < length; ++i) {
        ta[i] = 4;
    }

    ta.sort((a, b) => (a/4|0) - (b/4|0));

    assertEq(ta[0], 2);
    assertEq(ta[1], 1);
    assertEq(ta[2], 0);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
