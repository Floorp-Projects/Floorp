// Test 1.
function g(f, inputs) {
    for (var j = 0; j < 2; j++) {
        try {
            f(inputs[j])
        } catch (e) {}
    }
}
function f(x) {
    returnx > 0 ? x && Number.MIN_VALUE >> 0 : x;
};
g(f, [-Number.E])

// Test 2.
function h(x) {
    (x && 4294967296 >> 0)()
}
try {
    h(Number.MAX_VALUE)
    h()
} catch (e) {}

// Test 3.
var arr = new Float64Array([1, 2, 3, 4, 5, 6, 7, 8, 9, -0]);
for (var i = 0; i < 10; i++)
{
    var el = +arr[i];
    print(uneval(el ? +0 : el));
}

// Test 4.
setIonCheckGraphCoherency()
function j(x) {
    x(Math.hypot(x && 0, 4294967296))
}
try {
    j(Infinity)
    j()
} catch (e) {}
