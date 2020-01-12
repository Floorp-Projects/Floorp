// |jit-test| --disable-tosource

const TEST_CASES = [
    [undefined, "(void 0)"],
    [null, "null"],
    [true, "true"],
    [Symbol("abc"), `Symbol("abc")`],
    [15, "15"],
    [-0, "-0"],
    ["abc", `"abc"`],
    [function a() { return 1; }, `(function a() { return 1; })`],
    [[1, 2, 3], `({0:1, 1:2, 2:3})`], // This will be improved
    [new Error("Test"), `({})`],
]

for (let [actual, expected] of TEST_CASES) {
    assertEq(valueToSource(actual), expected);
}
