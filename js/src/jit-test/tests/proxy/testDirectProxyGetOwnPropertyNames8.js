// Return the names returned by the trap
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: true
});
var names = Object.getOwnPropertyNames(new Proxy(target, {
    ownKeys: function (target) {
        return [ 'bar' ];
    }
}));
assertEq(names.length, 1);
assertEq(names[0], 'bar');

var names = Object.getOwnPropertyNames(new Proxy(Object.create(Object.create(null, {
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
}), {
    ownKeys: function (target) {
        return [ 'c', 'e' ];
    }
}));
assertEq(names.length, 2);
assertEq(names[0], 'c');
assertEq(names[1], 'e');
