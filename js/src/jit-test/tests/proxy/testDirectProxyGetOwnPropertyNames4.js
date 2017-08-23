load(libdir + "asserts.js");

// Throw TypeError if trap returns a property key multiple times.

var handler = { ownKeys : () => [ 'foo', 'foo' ] };
for (let p of [new Proxy({}, handler), Proxy.revocable({}, handler).proxy])
    assertThrowsInstanceOf(() => Object.getOwnPropertyNames(p), TypeError);
