function SameValue(x, y) {
    if (x === y) {
        return (x !== 0) || (1 / x === 1 / y);
    }
    return (x !== x && y !== y);
}

function* cartesian(a, b = a) {
    for (var pa of a) {
        for (var pb of b) {
            yield [pa, pb];
        }
    }
}

var testValues = {
    Double: `[-0.0, +0.0, 1.0, NaN]`,
    Int32: `[-1, 0, 1, 2]`,
    Value: `[-0.0, +0.0, "", NaN]`,
};

for (var [xs, ys] of cartesian(Object.values(testValues))) {
    var fn = Function(`
        var xs = ${xs};
        var ys = ${ys};
        assertEq(xs.length, 4);
        assertEq(ys.length, 4);

        // Compare each entry in xs with each entry in ys and ensure Object.is
        // computes the same result as SameValue.
        var actual = 0, expected = 0;
        for (var i = 0; i < 1000; ++i) {
            // 0 1 2 3
            var xi = i & 3;

            // 0 1 2 3  1 2 3 0  2 3 0 1  3 0 1 2
            var yi = (i + ((i >> 2) & 3)) & 3;

            actual += Object.is(xs[xi], ys[yi]) ? 1 : 0;
            expected += SameValue(xs[xi], ys[yi]) ? 1 : 0;
        }
        assertEq(actual, expected);
    `);
    for (var i = 0; i < 3; ++i) {
        fn();
    }
}

