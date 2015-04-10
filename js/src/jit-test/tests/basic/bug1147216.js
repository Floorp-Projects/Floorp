// Ensure JSOP_LINENO (emitted after JSOP_EVAL) handles big line
// numbers correctly.
function f() {
    var s = "";
    var stack;
    for (var i=0; i<66002; i++) {
	s += "\n";
	if (i === 66000)
	    s += "eval('stack = Error().stack');";
    }
    eval(s);
    assertEq(stack.indexOf("line 66002") > 0, true);
}
f();
