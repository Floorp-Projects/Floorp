var otherGlobalSameCompartment = newGlobal("same-compartment");
eval = otherGlobalSameCompartment.eval;
doesNotNeedParens(1, "if (xx) { }");
needParens(2, "if (1, xx) { }");
function doesNotNeedParens(section, pat) {
    try {
        f = new Function
    } catch (e) {}
    roundTripTest(section, f)
}
function needParens(section, pat, exp) {
    var f, ft;
    roundTripTest(section, f, exp)
}
function roundTripTest(section, f, exp) {
    uf = "" + f
    var euf;
    try {
      euf = eval("(" + uf + ")");
    } catch (e) { }
    + euf
}
