// Forward to the target if the trap is not defined
var names = Object.getOwnPropertyNames(Proxy(Object.create(Object.create(null, {
    a: {
        enumerable: true,
        configurable: true
    },
    b: {
        enumerable: false,
        configurable: true
    }
}), {
    c: {
        enumerable: true,
        configurable: true
    },
    d: {
        enumerable: false,
        configurable: true
    }
}), {}));
assertEq(names.length, 2);
assertEq(names[0], 'c');
assertEq(names[1], 'd');
