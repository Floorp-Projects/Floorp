// Forward to the target if the trap is not defined
var target = {
    foo: 'bar'
};
for (let p of [new Proxy(target, {}), Proxy.revocable(target, {}).proxy]) {
    // The sets from the first iteration will affect target, but it doesn't
    // matter, since the effectiveness of the foo sets is still observable.
    p.foo = 'baz';
    assertEq(target.foo, 'baz');
    p['foo'] = 'buz';
    assertEq(target.foo, 'buz');

    if (typeof Symbol === "function") {
        var sym = Symbol.for('quux');
        p[sym] = sym;
        assertEq(target[sym], sym);
        // Reset for second iteration
        target[sym] = undefined;
    }
}
