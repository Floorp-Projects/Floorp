assertEq(Array.isArray([]), true);

var proxy = new Proxy([], {});
assertEq(Array.isArray(proxy), true);

for (var i = 0; i < 10; i++) {
    proxy = new Proxy(proxy, {});
    assertEq(Array.isArray(proxy), true);
}

var revocable = Proxy.revocable([], {});
proxy = revocable.proxy;
assertEq(Array.isArray(proxy), true);

for (var i = 0; i < 10; i++) {
    proxy = new Proxy(proxy, {});
    assertEq(Array.isArray(proxy), true);
}

revocable.revoke();
assertEq(Array.isArray(revocable.proxy), false);
assertEq(Array.isArray(proxy), false);

var global = newGlobal();
var array = global.Array();
assertEq(Array.isArray(array), true);
assertEq(Array.isArray(new Proxy(array, {})), true);
assertEq(Array.isArray(new global.Proxy(array, {})), true);

reportCompare(true, true);
