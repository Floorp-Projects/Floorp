// |jit-test| error:TypeError
var otherGlobal = newGlobal();
var proxy = new (otherGlobal.Proxy)({}, {});
Int8Array.set(proxy);
