
try { eval("3 ** 4") } catch (e if e instanceof SyntaxError) { quit(); };

var f = new Function("x", "return (x ** (1 / ~4294967297)) && x");
for (var i = 0; i < 2; ++i) {
    assertEq(f(-Infinity), 0);
}
