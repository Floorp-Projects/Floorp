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
    [[1, 2, 3], `[1, 2, 3]`],
    [[1, {a: 0, b: 0}, 2], `[1, {a:0, b:0}, 2]`],
    [{a: [1, 2, 3]}, `({a:[1, 2, 3]})`],
    [new Error("Test"), `({})`], // This will be improved
]

for (let [actual, expected] of TEST_CASES) {
    assertEq(valueToSource(actual), expected);
}
