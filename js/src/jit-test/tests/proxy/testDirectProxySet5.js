// Reflect side-effects from the trap
var target = {
    foo: 'bar'
};
new Proxy(target, {
    set: function (target, name, val, receiver) {
        target[name] = 'qux';
    }
})['foo'] = 'baz';
assertEq(target['foo'], 'qux');
