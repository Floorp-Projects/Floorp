// Testcase 1.
try {
    function g(x) {
        (x | 0 && 0)()
    }
    (function(f, s) {
        f()
    })(g, [])
} catch (e) {}

// Testcase 2.
function g2(f, inputs) {
    for (var j = 0; j < 49; ++j) {
        for (var k = 0; k < 49; ++k) {
            try {
                f()
            } catch (e) {}
        }
    }
}
function f1(x, y) {
    (x | 0 ? Number.MAX_VALUE | 0 : x | 0)();
};
function f2(y) {
    f1(y | 0)();
};
g2(f2, [Number])

// Testcase 3.
function h(f) {
    for (var j = 0; j < 99; ++j) {
        for (var k = 0; k < 99; ++k) {
            try {
                f()
            } catch (e) {}
        }
    }
}
function g3(x) {
    (x | 0 ? Number.MAX_VALUE | 0 : x | 0)
}
h(g3, [Number])

// Testcase 4.
function m(f) {
    f()
}
function g4(x) {
    return x ? Math.fround(-Number.MIN_VALUE) : x
}
m(g4)
function h2(M) {
    try {
    (g4(-0 + M))()
    } catch (e) {}
}
m(h2, [Math - Number])

