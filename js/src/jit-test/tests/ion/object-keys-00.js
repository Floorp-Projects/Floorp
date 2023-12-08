// This test case is used to test the optimized code path where the computation
// of `Object.keys(...).length` no longer compute `Object.keys(...)` as an
// intermediate result.
//
// This test verifies that the result remain consistent after the optimization.

function cmp_keys_length(a, b) {
    return Object.keys(a).length == Object.keys(b).length;
}

let objs = [
    {x:0, a: 1, b: 2},
    {x:1, b: 1, c: 2},
    {x:2, c: 1, d: 2},
    {x:3, a: 1, b: 2, c: 3},
    {x:4, b: 1, c: 2, d: 3},
    {x:5, a: 1, b: 2, c: 3, d: 4}
];

function max_of(o) {
    return o?.d ?? o?.c ?? o?.b ?? 0;
}

with({}) {}
for (let i = 0; i < 1000; i++) {
    for (let o1 of objs) {
        for (let o2 of objs) {
            assertEq(cmp_keys_length(o1, o2), max_of(o1) == max_of(o2));
        }
    }
}
