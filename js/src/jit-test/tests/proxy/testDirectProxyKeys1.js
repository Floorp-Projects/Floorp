// Forward to the target if the trap is not defined
var proto = Object.create(null, {
    a: {
        enumerable: true,
        configurable: true
    },
    b: {
        enumerable: false,
        configurable: true
    }
});
var target = Object.create(proto, {
    c: {
        enumerable: true,
        configurable: true
    },
    d: {
        enumerable: false,
        configurable: true
    }
});

for (let p of [new Proxy(target, {}), Proxy.revocable(target, {}).proxy]) {
    var names = Object.keys(p);
    assertEq(names.length, 1);
    assertEq(names[0], 'c');
}
