// Reduced from v8-raytrace.

var Class = {
	create : function() {
		return function() {
			this.initialize.apply(this, arguments);
		}
	}
}

var Bar = Class.create();
Bar.prototype = {
	// Compiled third.
	initialize : function() { }
}

var Foo = Class.create();
Foo.prototype = {
	// Compiled second. Crashes when setting "bar". Uses LCallConstructor.
	initialize : function() {
		this.bar = new Bar();
	}
}

// Compiled first.
function f() {
	for (var i = 0; i < 100; i++) {
		var foo = new Foo();
	}
}

f();
