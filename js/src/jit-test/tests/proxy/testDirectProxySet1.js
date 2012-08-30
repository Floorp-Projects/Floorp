// Forward to the target if the trap is not defined
var target = {
    foo: 'bar'
};
new Proxy(target, {})['foo'] = 'baz';
assertEq(target.foo, 'baz');
