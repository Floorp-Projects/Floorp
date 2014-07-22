load(libdir + "asserts.js");

/*
 * Throw a TypeError if the trap returns a non-configurable descriptor for a
 * non-existent property
 */
var handler = { getOwnPropertyDescriptor: () => ({ configurable: false }) };
for (let p of [new Proxy({}, handler), Proxy.revocable({}, handler).proxy])
    assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(p, 'foo'), TypeError);
