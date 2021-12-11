// Forward to the target if the trap is undefined
var p;
var target = function (x, y) {
    assertEq(new.target, p);
    this.foo = x + y;
}

for (p of [new Proxy(target, {}), Proxy.revocable(target, {}).proxy]) {
    var obj = new p(2, 3);
    assertEq(obj.foo, 5);
    assertEq(Object.getPrototypeOf(obj), target.prototype);
}
