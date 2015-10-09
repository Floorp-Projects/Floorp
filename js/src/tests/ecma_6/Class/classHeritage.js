var test = `

// It's an error to have a non-constructor as your heritage
assertThrowsInstanceOf(() => eval(\`class a extends Math.sin {
                                        constructor() { }
                                    }\`), TypeError);
assertThrowsInstanceOf(() => eval(\`(class a extends Math.sin {
                                        constructor() { }
                                    })\`), TypeError);

// Unless it's null, in which case it works like a normal class, except that
// the prototype object does not inherit from Object.prototype.
class basic {
    constructor() { }
}
class nullExtends extends null {
    constructor() { }
}
var nullExtendsExpr = class extends null { constructor() { } };

assertEq(Object.getPrototypeOf(basic), Function.prototype);
assertEq(Object.getPrototypeOf(basic.prototype), Object.prototype);

for (let extension of [nullExtends, nullExtendsExpr]) {
    assertEq(Object.getPrototypeOf(extension), Function.prototype);
    assertEq(Object.getPrototypeOf(extension.prototype), null);
}

var baseMethodCalled;
var staticMethodCalled;
var overrideCalled;
class base {
    constructor() { };
    method() { baseMethodCalled = true; }
    static staticMethod() { staticMethodCalled = true; }
    override() { overrideCalled = "base" }
}
class derived extends base {
    constructor() { super(); };
    override() { overrideCalled = "derived"; }
}
var derivedExpr = class extends base {
    constructor() { super(); };
    override() { overrideCalled = "derived"; }
};

// Make sure we get the right object layouts.
for (let extension of [derived, derivedExpr]) {
    baseMethodCalled = false;
    staticMethodCalled = false;
    overrideCalled = "";
    // Make sure we get the right object layouts.
    assertEq(Object.getPrototypeOf(extension), base);
    assertEq(Object.getPrototypeOf(extension.prototype), base.prototype);

    // We do inherit the methods, right?
    (new extension()).method();
    assertEq(baseMethodCalled, true);

    // But we can still override them?
    (new extension()).override();
    assertEq(overrideCalled, "derived");

    // What about the statics?
    extension.staticMethod();
    assertEq(staticMethodCalled, true);
}

// Gotta extend an object, or null.
function nope() {
    class Foo extends "Bar" {
        constructor() { }
    }
}
function nopeExpr() {
    (class extends "Bar" {
        constructor() { }
     });
}
assertThrowsInstanceOf(nope, TypeError);
assertThrowsInstanceOf(nopeExpr, TypeError);

// The .prototype of the extension must be an object, or null.
nope.prototype = "not really, no";
function stillNo() {
    class Foo extends nope {
        constructor() { }
    }
}
function stillNoExpr() {
    (class extends nope {
        constructor() { }
     });
}
assertThrowsInstanceOf(stillNo, TypeError);
assertThrowsInstanceOf(stillNoExpr, TypeError);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
