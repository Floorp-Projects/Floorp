/*
 * Call the trap with the handler as the this value, the target as the first
 * argument, the name of the property as the second argument, and the receiver
 * as the third argument
 */
var target = {};
for (var key of ['foo', Symbol.iterator]) {
    handler = {};
    for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
        handler.get =
            function (target1, name, receiver) {
                assertEq(this, handler);
                assertEq(target1, target);
                assertEq(name, key);
                assertEq(receiver, p);
                called = true;
            };
        var called = false;
        assertEq(p[key], undefined);
        assertEq(called, true);
    }
}
