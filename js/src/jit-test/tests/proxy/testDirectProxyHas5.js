// Return the trap result
var proto = Object.create(null, {
    'foo': {
        configurable: true
    }
});
var target = Object.create(proto, {
    'bar': {
        configurable: true
    }
});

var handler = { has: () => false };

for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
    assertEq('foo' in p, false);
    assertEq('bar' in p, false);
}
