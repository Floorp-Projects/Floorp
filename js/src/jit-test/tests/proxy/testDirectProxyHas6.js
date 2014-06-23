/*
 * Don't throw a type error if the trap reports an undefined property as
 * non-present, regardless of extensibility.
 */
var target = {};
Object.preventExtensions(target);
var p = new Proxy(target, {
    has: function (target, name) {
        return false;
    }
});
assertEq('foo' in p, false);
assertEq(Symbol.iterator in p, false);
