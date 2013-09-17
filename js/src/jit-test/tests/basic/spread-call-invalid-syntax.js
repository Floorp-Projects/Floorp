load(libdir + "asserts.js");

var offenders = [
    "f(1 ... n)",
    "f(...x for (x in y))",
    "f(...)",
    "f(...,)",
    "f(... ...[])",
    "f(...x,)",
    "f(x, ...)",
    "f(...x, x for (x in y))",
    "f(x for (x in y), ...x)"
];
for (var sample of offenders) {
    assertThrowsInstanceOf(function() { eval(sample); }, SyntaxError);
}
