load(libdir + "asserts.js");

var handler = { ownKeys: () => [ 'foo', 'foo' ] };
for (let p of [new Proxy({}, handler), Proxy.revocable({}, handler).proxy])
    assertDeepEq(Object.keys(p), []); // Properties are not enumerable.
