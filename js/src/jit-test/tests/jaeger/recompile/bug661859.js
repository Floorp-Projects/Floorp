function TestCase(n, d, e, a) this.expect = e;
function reportCompare(expected, actual, description) {
    typeof actual
}
expect = 1;
var summary = 'Do not assert: top < ss->printer->script->depth';
var actual = 'No Crash';
var expect = 'No Crash';
test();
function notInlined(f) {
    // prevent inlining this function, as a consequence, it prevent inlining
    // Array.prototype.some (Bug 1087468)
    with ({}) {}
    return f;
}
function test(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) {
    try {
        p = [1].some(notInlined(function (y) test())) ? 4 : 0x0041;
    } catch (ex) {}
    reportCompare(expect, actual, summary)
}
test();
TestCase();
test()
