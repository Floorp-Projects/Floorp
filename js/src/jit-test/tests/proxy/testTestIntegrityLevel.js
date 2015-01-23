// isSealed/isFrozen should call getOwnPropertyDescriptor for all properties.

var log = [];
var target = Object.preventExtensions({a: 1, b: 2, c: 3});
var p = new Proxy(target, {
    getOwnPropertyDescriptor(t, id) {
        log.push(id);
        return Object.getOwnPropertyDescriptor(t, id);
    }
});
assertEq(Object.isSealed(p), false);
assertEq(log.sort().join(''), 'abc');

log.length = 0;
assertEq(Object.isFrozen(p), false);
assertEq(log.sort().join(''), 'abc');
