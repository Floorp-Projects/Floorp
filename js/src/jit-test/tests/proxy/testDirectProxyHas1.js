// Forward to the target if the trap is not defined
var proxy = Proxy(Object.create(Object.create(null, {
    'foo': {
        configurable: true
    }
}), {
    'bar': {
        configurable: true
    }
}), {});
assertEq('foo' in proxy, true);
assertEq('bar' in proxy, true);
assertEq('baz' in proxy, false);
assertEq(Symbol() in proxy, false);
