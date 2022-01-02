// Call an Ion constructor from the interpreter.

// This gets compiled and called by the interpreter.
// Allocation and primitive check need to happen caller-side.
function Foo() {
	this.x = 5;
	return 4;
}

eval("//nothing"); // Prevent compilation of global script.

for (var i = 0; i < 100; i++) {
	var x = new Foo();
	assertEq(typeof(x), "object");
}
