// Forward to the target if the trap is not defined
var target = {
    foo: 'bar'
};
Proxy(target, {})['foo'] = 'baz';
assertEq(target.foo, 'baz');
