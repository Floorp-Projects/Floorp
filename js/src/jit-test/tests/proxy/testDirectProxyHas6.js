/*
 * Don't throw a type error if the trap reports an undefined property as
 * non-present, regardless of extensibility.
 */
var target = {};
Object.preventExtensions(target);
assertEq(
    'foo' in new Proxy(target, {
        has: function (target, name) {
            return false;
        }
    }), false);
