// Forward to the target if the trap is not defined
var target = { foo: 'bar' };
for (let p of [new Proxy(target, {}), Proxy.revocable(target, {}).proxy]) {
    assertEq(p.foo, 'bar');
    assertEq(p['foo'], 'bar');
}

if (typeof Symbol === "function") {
    var s = Symbol.for("moon");
    var obj = {};
    obj[s] = "dust";
    for (let p of [new Proxy(obj, {}), Proxy.revocable(obj, {}).proxy])
        assertEq(p[s], "dust");
}
