// |jit-test| skip-if: getBuildConfiguration('pbl')
g13 = newGlobal({newCompartment: true})
g13.parent = this;
g13.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) {
	frame.older;
  }
} + ")()");

function foo(depth) {
    try {
	if (depth > 0) {
	    bar(depth - arguments.length);
	} else {
	    throw 1;
	}
    } catch (e) { throw e }
}
function bar(depth) {
    foo(depth);
}

try {
    foo(50);
} catch {}
