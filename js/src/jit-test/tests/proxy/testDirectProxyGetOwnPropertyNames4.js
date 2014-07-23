load(libdir + "asserts.js");

// Throw a TypeError if the trap reports the same property twice
var handler = { ownKeys : () => [ 'foo', 'foo' ] };
for (let p of [new Proxy({}, handler), Proxy.revocable({}, handler).proxy])
    assertThrowsInstanceOf(() => Object.getOwnPropertyNames(p), TypeError);
