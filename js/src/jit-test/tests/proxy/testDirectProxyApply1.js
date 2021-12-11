// Forward to the target if the trap is undefined
var target = function (x, y) {
    return x + y;
}
for (let p of [new Proxy(target, {}), Proxy.revocable(target, {}).proxy])
    assertEq(p(2, 3), 5);
