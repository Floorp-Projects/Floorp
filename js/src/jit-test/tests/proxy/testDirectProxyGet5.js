// Return the trap result
assertEq(new Proxy({
    foo: 'bar'
}, {
    get: function (target, name, receiver) {
        return 'baz';
    }
}).foo, 'baz');

assertEq(new Proxy({
    foo: 'bar'
}, {
    get: function (target, name, receiver) {
        return undefined;
    }
}).foo, undefined);
