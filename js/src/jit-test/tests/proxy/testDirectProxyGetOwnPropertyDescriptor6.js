// Return undefined if the trap returns undefined
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: true
});
assertEq(Object.getOwnPropertyDescriptor(new Proxy(target, {
    getOwnPropertyDescriptor: function (target, name) {
        return undefined;
    }
}), 'foo'), undefined);
