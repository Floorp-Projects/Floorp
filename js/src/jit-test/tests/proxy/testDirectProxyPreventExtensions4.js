load(libdir + "asserts.js");

// Throw a TypeError if the object refuses to be made non-extensible
var handler = { preventExtensions: () => false };
for (let p of [new Proxy({}, handler), Proxy.revocable({}, handler).proxy])
    assertThrowsInstanceOf(() => Object.preventExtensions(p), TypeError);
