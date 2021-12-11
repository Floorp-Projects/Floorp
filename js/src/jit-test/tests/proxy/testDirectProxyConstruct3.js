// Return the trap result
function setFoo(x,y) { this.foo = x + y; }
var handler = { construct: function (target, args) { return { foo : args[0] * args[1]}; } }

for (let proxy of [new Proxy(setFoo, handler), Proxy.revocable(setFoo, handler).proxy]) {
    var obj1 = new proxy(2, 3);
    assertEq(obj1.foo, 6);
    obj1.bar = proxy;
    var obj2 = new obj1.bar(2, 3);
    assertEq(obj2.foo, 6);
}
