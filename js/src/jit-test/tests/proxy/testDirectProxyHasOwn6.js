// Return the trap result
var proxy = new Proxy(Object.create(Object.create(null, {
    'foo': {
        configurable: true
    }
}), {
    'bar': {
        configurable: true
    }
}), {
    hasOwn: function (target, name) {
        return name == 'foo';
    }
});
assertEq(({}).hasOwnProperty.call(proxy, 'foo'), true);
assertEq(({}).hasOwnProperty.call(proxy, 'bar'), false);
