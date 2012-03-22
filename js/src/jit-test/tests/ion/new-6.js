// Enter an Ion constructor via on-stack replacement.

// This gets compiled and called by the interpreter.
// Allocation and primitive check need to happen caller-side.
function Foo() {
	var y = 0;
	for (var i = 0; i < 100; i++)
		{ y++ }
	this.x = 5;
	return y;
}

eval("//nothing"); // Prevent compilation of global script.

for (var i = 0; i < 100; i++) {
	var x = new Foo();
	assertEq(typeof(x), "object");
}
