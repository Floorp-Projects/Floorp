// Handle bailing from a constructor.

var confuzzle = 0;

function BailFromConstructor() {
	this.x = "cats";
	this.y = confuzzle + 5;
	return 4;
}

function f() {
	var x;
	for (var i = 0; i < 100; i++) {
		if (i == 99)
			confuzzle = undefined;
		x = new BailFromConstructor();
		assertEq(typeof(x), "object");
	}
}

f();
