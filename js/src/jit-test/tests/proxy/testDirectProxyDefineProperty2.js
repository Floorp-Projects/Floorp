/*
 * Call the trap with the handler as the this value, the target as the first
 * argument, the name of the property as the second argument, and the descriptor
 * as the third argument.
 */
var target = {};
var log = [];
var handler = {
    defineProperty: function (target1, key, desc1) {
        assertEq(this, handler);
        assertEq(target1, target);
        log.push(key);
        assertEq(desc1 == desc, false);
        assertEq(desc1.value, 'bar');
        assertEq(desc1.writable, true);
        assertEq(desc1.enumerable, false);
        assertEq(desc1.configurable, true);
        called = true;
    }
}
var desc = {
    value: 'bar',
    writable: true,
    enumerable: false,
    configurable: true
};

var p = new Proxy(target, handler);
Object.defineProperty(p, 'foo', desc);
Object.defineProperty(p, Symbol.for('quux'), desc);
assertEq(log.length, 2);
assertEq(log[0], 'foo');
assertEq(log[1], Symbol.for('quux'));
assertEq(Object.isExtensible(target), true);
assertEq(Object.isExtensible(p), true);
Object.preventExtensions(target);
assertEq(Object.isExtensible(target), false);
assertEq(Object.isExtensible(p), false);
