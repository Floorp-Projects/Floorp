// Forward to the target if the trap is not defined
var target = {};
Object.defineProperty(target, 'foo', {
    value: 'bar',
    writable: true,
    enumerable: false,
    configurable: true
});
var desc = Object.getOwnPropertyDescriptor(Proxy(target, {}), 'foo');
assertEq(desc.value, 'bar');
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

var proto = {};
Object.defineProperty(proto, 'foo', {
    value: 'bar',
    writable: true,
    enumerable: false,
    configurable: true
});
var target = Object.create(proto);
assertEq(Object.getOwnPropertyDescriptor(Proxy(target, {}), 'foo'), undefined);
