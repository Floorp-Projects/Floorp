// Return the descriptor returned by the trap
var target = {};
Object.defineProperty(target, 'foo', {
    value: 'bar',
    writable: true,
    enumerable: false,
    configurable: true
});
var desc = {
    value: 'baz',
    writable: false,
    enumerable: true,
    configurable: true
};
var desc1 = Object.getOwnPropertyDescriptor(new Proxy(target, {
    getOwnPropertyDescriptor: function (target, name) {
        return desc;
    }
}), 'foo');
assertEq(desc1 == desc, false);
assertEq(desc1.value, 'baz');
assertEq(desc1.writable, false);
assertEq(desc1.enumerable, true);
assertEq(desc1.configurable, true);

// The returned descriptor must agree in configurability.
var desc = { configurable : true };
var desc1 = Object.getOwnPropertyDescriptor(new Proxy(target, {
    getOwnPropertyDescriptor: function (target, name) {
        return desc;
    }
}), 'foo');
assertEq(desc1 == desc, false);
assertEq(desc1.value, undefined);
assertEq(desc1.writable, false);
assertEq(desc1.enumerable, false);
assertEq(desc1.configurable, true);
