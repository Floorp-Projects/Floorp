function foo() {
    x = null;
}
function f() {
    for (var i=0; i<99; i++) {
	x = null;
	if (i >= 97) {
	    gc();
	    gc();
	    foo();
	}
	x = {};
	if (i >= 97)
	    foo();
    }
}
f();
