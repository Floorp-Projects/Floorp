// String.prototype[@@iterator] and StringIterator.prototype surface tests

load(libdir + "array-compare.js");
load(libdir + "asserts.js");
load(libdir + "iteration.js");

function assertDataDescriptor(actual, expected) {
    assertEq(actual.value, expected.value);
    assertEq(actual.writable, expected.writable);
    assertEq(actual.enumerable, expected.enumerable);
    assertEq(actual.configurable, expected.configurable);
}

function isConstructor(o) {
    try {
        new (new Proxy(o, {construct: () => ({})}));
        return true;
    } catch(e) {
        return false;
    }
}

function assertBuiltinFunction(o, name, arity) {
    var fn = o[name];
    assertDataDescriptor(Object.getOwnPropertyDescriptor(o, name), {
        value: fn,
        writable: true,
        enumerable: false,
        configurable: true,
    });

    assertEq(typeof fn, "function");
    assertEq(Object.getPrototypeOf(fn), Function.prototype);
    assertEq(isConstructor(fn), false);

    assertEq(arraysEqual(Object.getOwnPropertyNames(fn).sort(), ["length", "name"].sort()), true);

    assertDataDescriptor(Object.getOwnPropertyDescriptor(fn, "length"), {
        value: arity,
        writable: false,
        enumerable: false,
        configurable: true
    });

    var functionName = typeof name === "symbol"
                       ? String(name).replace(/^Symbol\((.+)\)$/, "[$1]")
                       : name;
    assertDataDescriptor(Object.getOwnPropertyDescriptor(fn, "name"), {
        value: functionName,
        writable: false,
        enumerable: false,
        configurable: true
    });
}


// String.prototype[@@iterator] is a built-in function
assertBuiltinFunction(String.prototype, Symbol.iterator, 0);

// Test StringIterator.prototype surface
var iter = ""[Symbol.iterator]();
var iterProto = Object.getPrototypeOf(iter);

// StringIterator.prototype inherits from %IteratorPrototype%. Check it's the
// same object as %ArrayIteratorPrototype%'s proto.
assertEq(Object.getPrototypeOf(iterProto),
         Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));

// Own properties for StringIterator.prototype: "next"
assertEq(arraysEqual(Object.getOwnPropertyNames(iterProto).sort(), ["next"]), true);

// StringIterator.prototype.next is a built-in function
assertBuiltinFunction(iterProto, "next", 0);

// StringIterator.prototype[@@iterator] is generic and returns |this|
for (var v of [void 0, null, true, false, "", 0, 1, {}, [], iter, iterProto]) {
    assertEq(iterProto[Symbol.iterator].call(v), v);
}

// StringIterator.prototype.next is not generic
for (var v of [void 0, null, true, false, "", 0, 1, {}, [], iterProto]) {
    assertThrowsInstanceOf(() => iterProto.next.call(v), TypeError);
}
