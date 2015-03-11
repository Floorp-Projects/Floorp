var test = `

// It's an error to have a non-constructor as your heritage
assertThrowsInstanceOf(() => eval(\`class a extends Math.sin {
                                        constructor() { }
                                    }\`), TypeError);

// Unless it's null, in which case it works like a normal class, except that
// the prototype object does not inherit from Object.prototype.
class basic {
    constructor() { }
}
class nullExtends extends null {
    constructor() { }
}
assertEq(Object.getPrototypeOf(basic), Function.prototype);
assertEq(Object.getPrototypeOf(nullExtends), Function.prototype);
assertEq(Object.getPrototypeOf(basic.prototype), Object.prototype);
assertEq(Object.getPrototypeOf(nullExtends.prototype), null);

var baseMethodCalled = false;
var staticMethodCalled = false;
var overrideCalled = "";
class base {
    constructor() { };
    method() { baseMethodCalled = true; }
    static staticMethod() { staticMethodCalled = true; }
    override() { overrideCalled = "base" }
}
class derived extends base {
    constructor() { };
    override() { overrideCalled = "derived"; }
}

// Make sure we get the right object layouts.
assertEq(Object.getPrototypeOf(derived), base);
assertEq(Object.getPrototypeOf(derived.prototype), base.prototype);

// We do inherit the methods, right?
(new derived()).method();
assertEq(baseMethodCalled, true);

// But we can still override them?
(new derived()).override();
assertEq(overrideCalled, "derived");

// What about the statics?
derived.staticMethod();
assertEq(staticMethodCalled, true);

// Gotta extend an object, or null.
function nope() {
    class Foo extends "Bar" {
        constructor() { }
    }
}
assertThrowsInstanceOf(nope, TypeError);

// The .prototype of the extension must be an object, or null.
nope.prototype = "not really, no";
function stillNo() {
    class Foo extends nope {
        constructor() { }
    }
}
assertThrowsInstanceOf(stillNo, TypeError);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
