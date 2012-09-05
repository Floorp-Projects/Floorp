// Forward to the target if the trap is not defined
var proxy = new Proxy(Object.create(Object.create(null, {
    'foo': {
        configurable: true
    }
}), {
    'bar': {
        configurable: true
    }
}), {});
assertEq(({}).hasOwnProperty.call(proxy, 'foo'), false);
assertEq(({}).hasOwnProperty.call(proxy, 'bar'), true);
