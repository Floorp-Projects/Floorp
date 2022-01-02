// Return undefined if the trap returns undefined
var target = {};
Object.defineProperty(target, 'foo', {
    configurable: true
});

var handler = { getOwnPropertyDescriptor: () => undefined };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy])
    assertEq(Object.getOwnPropertyDescriptor(p, 'foo'), undefined);
