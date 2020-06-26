// Ion provides specialisations when Math.pow() resp. the **-operator is used
// with a constant power of one of [-0.5, 0.5, 1, 2, 3, 4].

function test(x, y, z) {
    function pow(x, y) { return `Math.pow(${x}, ${y})` };
    function exp(x, y) { return `((${x}) ** ${y})` };

    function make(fn, x, y, z) {
        return Function(`
            // Load from array to prevent constant-folding.
            // (Ion is currently not smart enough to realise that both array
            // values are the same.)
            var xs = [${x}, ${x}];
            var zs = [${z}, ${z}];
            for (var i = 0; i < 1000; ++i) {
                assertEq(${fn("xs[i & 1]", y)}, zs[i & 1]);
            }
        `);
    }

    function double(v) {
        // NB: numberToDouble() always returns a double value.
        return `numberToDouble(${v})`;
    }

    function addTests(fn) {
        tests.push(make(fn, x, y, z));
        tests.push(make(fn, x, double(y), z));
        tests.push(make(fn, double(x), y, z));
        tests.push(make(fn, double(x), double(y), z));
    }

    var tests = [];
    addTests(pow);
    addTests(exp);

    for (var i = 0; i < tests.length; ++i) {
        for (var j = 0; j < 2; ++j) {
            tests[i]();
        }
    }
}

// Make sure the tests below test int32 and double return values.

// Math.pow(x, -0.5)
test( 1, -0.5, 1);
test(16, -0.5, 0.25);

// Math.pow(x, 0.5)
test(16, 0.5, 4);
test( 2, 0.5, Math.SQRT2);

// Math.pow(x, 1)
test(5,   1, 5);
test(0.5, 1, 0.5);

// Math.pow(x, 2)
test(5,   2, 25);
test(0.5, 2, 0.25);

// Math.pow(x, 3)
test(5,   3, 125);
test(0.5, 3, 0.125);

// Math.pow(x, 3)
test(5,   4, 625);
test(0.5, 4, 0.0625);
