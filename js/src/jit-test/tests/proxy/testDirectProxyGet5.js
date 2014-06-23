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

var obj = {};
var s1 = Symbol("moon"), s2 = Symbol("sun");
obj[s1] = "wrong";
assertEq(new Proxy(obj, {
    get: () => s2
})[s1], s2);
