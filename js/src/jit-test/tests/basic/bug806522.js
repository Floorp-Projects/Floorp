load(libdir + "asserts.js");

var g = evalcx("lazy");
assertThrowsInstanceOf(
    () => evaluate("let (eval) {eval()}", {global: g}),
    g.TypeError); // eval is not a function
assertEq(evaluate("let (eval = parseInt) {eval()}", {global: g}), NaN);
