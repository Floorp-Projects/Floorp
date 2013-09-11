// |jit-test| error:TypeError
var otherGlobal = newGlobal();
var proxy = otherGlobal.Proxy.create({}, {});
Int8Array.set(proxy);
