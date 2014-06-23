// Forward to the target if the trap is not defined
var target = {
    foo: 'bar'
};
Proxy(target, {}).foo = 'baz';
assertEq(target.foo, 'baz');
Proxy(target, {})['foo'] = 'buz';
assertEq(target.foo, 'buz');

var sym = Symbol.for('quux');
Proxy(target, {})[sym] = sym;
assertEq(target[sym], sym);

