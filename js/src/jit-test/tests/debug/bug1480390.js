var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () {};");

function* wrapNoThrow() {
    let iter = {
	[Symbol.iterator]() {
            return this;
	},
	next() {
            return { value: 10, done: false };
	},
	return() { return "invalid return value" }
    };
    for (const i of iter)
	yield i;
}

function foo() {
    for (var i of [1,2,3]) {
	for (var j of [4,5,6]) {
	    try {
		for (const i of wrapNoThrow()) break;
	    } catch (e) {}
	}
	for (var j of [7,8,9]) {
	}
    }
}

for (var i = 0; i < 10; i++) {
    try {
	foo();
    } catch(e) {}
}
