load(libdir + "asserts.js");

var offenders = [
    "(1 ... n)",
    "[1 ... n]",
    "(...x)",
    "[...x for (x of y)]",
    "[...]",
    "(...)",
    "[...,]"
];
for (var sample of offenders) {
    assertThrowsInstanceOf(function () { eval(sample); }, SyntaxError);
}
