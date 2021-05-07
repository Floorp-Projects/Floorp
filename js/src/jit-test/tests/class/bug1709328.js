class A0 { constructor() { this.dummy = true; } }
class A1 { constructor() { this.dummy = true; } }
class A2 { constructor() { this.dummy = true; } }
class A3 { constructor() { this.dummy = true; } }
class A4 { constructor() { this.dummy = true; } }
class A5 { constructor() { this.dummy = true; } }
class A6 { constructor() { this.dummy = true; } }
class A7 { constructor() { this.dummy = true; } }
class A8 { constructor() { this.dummy = true; } }
class A9 { constructor() { this.dummy = true; } }

var constructors = [A1, A2, A3, A4, A5, A6, A7, A8, A9];
for (var i = 0; i < 1000; i++) {
    for (var construct of constructors) {
	var h = new construct();
	assertEq(Reflect.get(h, "nonexistent", "dummy"), undefined);
    }
}
