// Forward to the target if the trap is not defined
var target = {};
Object.defineProperty(Proxy(target, {}), 'foo', {
    value: 'bar',
    writable: true,
    enumerable: false,
    configurable: true
});
var desc = Object.getOwnPropertyDescriptor(target, 'foo');
assertEq(desc.value, 'bar');
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
