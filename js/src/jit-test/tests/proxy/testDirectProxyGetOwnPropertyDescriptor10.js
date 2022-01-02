// Return a new descriptor object that agrees with that returned by the trap
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
var handler = { getOwnPropertyDescriptor: function () { return desc; } };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
    var desc1 = Object.getOwnPropertyDescriptor(p, 'foo');
    assertEq(desc1 == desc, false);
    assertEq(desc1.value, 'baz');
    assertEq(desc1.writable, false);
    assertEq(desc1.enumerable, true);
    assertEq(desc1.configurable, true);
}

// The returned descriptor must agree in configurability.
desc = { configurable : true };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
    var desc1 = Object.getOwnPropertyDescriptor(p, 'foo');
    assertEq(desc1 == desc, false);
    assertEq(desc1.value, undefined);
    assertEq(desc1.writable, false);
    assertEq(desc1.enumerable, false);
    assertEq(desc1.configurable, true);
}
