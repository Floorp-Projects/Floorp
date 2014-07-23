load(libdir + "asserts.js");

// Throw a TypeError if the trap reports an extensible object as non-extensible

var handler = { preventExtensions: () => true };
for (let p of [new Proxy({}, handler), Proxy.revocable({}, handler).proxy])
    assertThrowsInstanceOf(() => Object.preventExtensions(p), TypeError);
