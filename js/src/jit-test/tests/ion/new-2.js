// Test JSOP_NEW using native constructors.
// Construct an object with a unique assignation to a property.
function f(i) {
	var x = new Number(i);
	return x;
}

// Assert that a unique object really was created.
for (var i = 0; i < 100; i++) {
	var o = f(i);
	assertEq(typeof o, "object");
	assertEq(Number(o), i);
}
