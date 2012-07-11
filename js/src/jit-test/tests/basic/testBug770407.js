// |jit-test| error:TypeError
var otherGlobal = newGlobal("new-compartment");
var proxy = otherGlobal.Proxy.create({}, {});
Int8Array.set(proxy);
