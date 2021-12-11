function f() {
    var s = "switch (x) {";
    for (var i=8000; i<16400; i++) {
	s += "case " + i + ": return " + i + "; break;";
    }
    s += "case 8005: return -1; break;";
    s += "}";
    var g = Function("x", s);
    assertEq(g(8005), 8005);
}
f();
