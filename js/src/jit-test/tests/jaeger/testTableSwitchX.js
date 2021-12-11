// Tests for JSOP_TABLESWITCHX.
function test1() {
    var src =
        "var a = 0;\n" +
        "switch(x) {\n";
    for (var i=-1; i<4; i++) {
        src += (i >= 0) ?
                "case " + i + ":\n" :
                "default:\n";
        for (var j=0; j<1500; j++) {
            src += "a = " + i + ";";
        }
        src += "break;\n";
    }
    src += "}\n";
    src += "return a;";

    var f = new Function("x", src);
    assertEq(f(0), 0);
    assertEq(f(4), -1);
    assertEq(f(), -1);
    assertEq(f(1.1), -1);
    assertEq(f(3), 3);
}
test1();
