// Ensure JSOP_LINENO (emitted after JSOP_EVAL) handles big line
// numbers correctly.
function getsource() {
    var s = "";
    for (var i=0; i<66002; i++) {
	s += "\n";
	if (i === 66000)
	    s += "eval('stack = Error().stack');";
    }
    return s;
}
function test() {
    var stack;
    eval(getsource());
    assertEq(stack.indexOf("line 66002") > 0, true);
}
test();

function testStrict() {
    "use strict";
    var stack;
    eval(getsource());
    assertEq(stack.indexOf("line 66002") > 0, true);
}
testStrict();
