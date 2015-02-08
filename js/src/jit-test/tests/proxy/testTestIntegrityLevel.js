// isSealed/isFrozen should short-circuit.

var count = 0;
var target = Object.preventExtensions({a: 1, b: 2, c: 3});
var p = new Proxy(target, {
    getOwnPropertyDescriptor(t, id) {
        count++;
        return Object.getOwnPropertyDescriptor(t, id);
    }
});
assertEq(Object.isSealed(p), false);
assertEq(count, 1);

count = 0;
assertEq(Object.isFrozen(p), false);
assertEq(count, 1);

Object.seal(target);
count = 0;
assertEq(Object.isSealed(p), true);
assertEq(count, 3);

count = 0;
assertEq(Object.isFrozen(p), false);
assertEq(count, 1);

Object.freeze(target);
count = 0;
assertEq(Object.isFrozen(p), true);
assertEq(count, 3);
