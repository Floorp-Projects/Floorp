function tryItOut(code) {
    try {
        f = Function(code)
    } catch (r) {}
    v = tryRunning(f, code)
    x = String;
    for (e in v) {}
}
function tryRunning() {
    try {
        rv = f();
        return rv;
    } catch (r) {
        x = String;
    }
}
__proto__.__defineSetter__("x", function() {});
tryItOut("/()/;\"\"()");
tryItOut("}");
tryItOut("");
tryItOut("o");
tryItOut(")");
tryItOut("(");
tryItOut(")");
tryItOut("}");
tryItOut("}");
tryItOut(")");
tryItOut(")");
tryItOut("");
tryItOut("l;function u(){/j/}");
tryItOut("(");
tryItOut("t");
tryItOut("(");
tryItOut(")");
tryItOut("(");
tryItOut("");
tryItOut("{t:g}");
tryItOut("r");
tryItOut("p");
tryItOut("gc()");
tryItOut("verifybarriers()");
tryItOut("/**/yield");
