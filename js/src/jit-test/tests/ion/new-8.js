// Handle bailing from a constructor that's called from the interpreter.

function yesokhellothankyou() {
	return 5;
}

function BailFromConstructor() {
	this.x = "cats";
	this.y = 5;
	var z = yesokhellothankyou();

	// Causes a bailout for purposes of inlining at the LRecompileCheck.
	// Yep, this is great.
	for (var i = 0; i < 10500; i++) {
		x = 4;
	}

	return 4;
}

var x = new BailFromConstructor();
