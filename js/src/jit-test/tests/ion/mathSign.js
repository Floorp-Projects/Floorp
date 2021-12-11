// Test Math.sign() for IonMonkey.
// Requires --ion-eager to enter at the top of each loop.

var signDITests = [
    [0.49999999999999997, 1],
    [0.5, 1],
    [1.0, 1],
    [1.5, 1],
    [792.8, 1],
    [-0.1, -1],
    [-1.0001, -1],
    [-3.14, -1],
    [900000000000, 1],
    [+0, +0],
    [-Infinity, -1],
    [Infinity, 1],
];

var signDITests_bailout = [
    // Add a few 'double -> int' tests before the bailout.
    ...(function*(){ for (var i = 0; i < 50; ++i) yield* signDITests; })(),

    // Trigger bailout for negative zero.
    [-0, -0],
];

var signDTests = [
    [-0, -0],
    [0.49999999999999997, 1],
    [0.5, 1],
    [1.0, 1],
    [1.5, 1],
    [792.8, 1],
    [-0.1, -1],
    [-1.0001, -1],
    [-3.14, -1],
    [900000000000, 1],
    [-0, -0],
    [+0, +0],
    [-Infinity, -1],
    [Infinity, 1],
    [NaN, NaN],
];

var signITests = [
    [0, 0],
    [4, 1],
    [-1, -1],
    [-7, -1],
    [2147483647, 1],
    [-2147483648, -1],
];

// Typed functions to be compiled by Ion.
function signDI(x) { return Math.sign(x); }
function signDI_bailout(x) { return Math.sign(x); }
function signD(x) { return Math.sign(x); }
function signI(x) { return Math.sign(x); }

function test() {
    // Always run this function in the interpreter.
    with ({}) {}

    for (var i = 0; i < signDITests.length; i++)
        assertEq(signDI(signDITests[i][0]), signDITests[i][1]);
    for (var i = 0; i < signDITests_bailout.length; i++)
        assertEq(signDI_bailout(signDITests_bailout[i][0]), signDITests_bailout[i][1]);
    for (var i = 0; i < signDTests.length; i++)
        assertEq(signD(signDTests[i][0]), signDTests[i][1]);
    for (var i = 0; i < signITests.length; i++)
        assertEq(signI(signITests[i][0]), signITests[i][1]);
}

for (var i = 0; i < 40; i++)
    test();
