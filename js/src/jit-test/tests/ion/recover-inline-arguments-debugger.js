// |jit-test| --fast-warmup

g13 = newGlobal({newCompartment: true})
g13.parent = this;
g13.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) {
	while (frame.older) {
	    frame = frame.older;
	}
  }
} + ")()");

function triggerUnwind() {
    try {
	throw 1;
    } catch {}
}

function foo(depth) {
    var dummy = arguments;
    if (depth == 0) {
	triggerUnwind();
    } else {
	bar(depth - 1);
    }
}

function bar(depth) {
    foo(depth);
}

bar(50);
