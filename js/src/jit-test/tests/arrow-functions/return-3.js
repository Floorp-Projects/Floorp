// return exits the innermost enclosing arrow (not an enclosing arrow)

load(libdir + "asserts.js");

function f() {
    var g = a => [0, 1].map(x => { return x + a; });
    return g(13);
}

assertDeepEq(f(), [13, 14]);
