// Breaks with --ion -n. See bug 718122.

function Foo()
{ }

Foo.prototype.bar = function(){
	print("yes hello");
	return 5;
}

var x = new Foo();

function f(x) {
	// Enter Ion.
	for (var i=0; i < 41; i++);

	// At this point we have no type information for the GetPropertyCache below.
	// This causes the cache to be typed as Value.
	x.bar();
}

f(x);
