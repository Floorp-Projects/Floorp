/*
 * Call the trap with the handler as the this value, and the target as the first
 * argument
 */
var target = {};
var called;
var handler = {
    ownKeys: function (target1) {
        assertEq(this, handler);
        assertEq(target1, target);
        called = true;
        return [];
    }
};

for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
    called = false;
    Object.keys(new Proxy(target, handler));
    assertEq(called, true);
}
