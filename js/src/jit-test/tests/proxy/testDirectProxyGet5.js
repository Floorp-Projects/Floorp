// Return the trap result
var target = { foo: 'bar' };
var s1 = Symbol("moon"), s2 = Symbol("sun");
target[s1] = "wrong";

var handler = { };
for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
    handler.get = (() => 'baz');
    assertEq(p.foo, 'baz');

    handler.get = (() => undefined);
    assertEq(p.foo, undefined);

    handler.get = (() => s2);
    assertEq(p[s1], s2);
}
