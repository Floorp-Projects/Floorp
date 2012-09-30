// Reflect side-effects from the trap
target = {};
var proxy = new Proxy(target, {
    preventExtensions: function (target) {
        Object.preventExtensions(target);
        return true;
    }
});
Object.preventExtensions(proxy);
assertEq(Object.isExtensible(target), false);
assertEq(Object.isExtensible(proxy), false);

