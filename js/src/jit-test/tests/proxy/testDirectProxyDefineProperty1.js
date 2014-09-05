// Forward to the target if the trap is not defined

var target;
function testProxy(p, key) {
    Object.defineProperty(p, key, {
        value: 'bar',
        writable: true,
        enumerable: false,
        configurable: true
    });
    var desc = Object.getOwnPropertyDescriptor(target, key);
    assertEq(desc.value, 'bar');
    assertEq(desc.writable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, true);
}

var keys = ['foo'];
if (typeof Symbol === "function")
    keys.push(Symbol("quux"));
for (var key of keys) {
    target = {};
    testProxy(new Proxy(target, {}), key);
    target = {};
    testProxy(Proxy.revocable(target, {}).proxy, key);
}
