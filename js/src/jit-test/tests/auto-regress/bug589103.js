// |jit-test| error:InternalError

// Binary: cache/js-dbg-64-b22e82ce2364-linux
// Flags:
//
load(libdir + "immutable-prototype.js");

if (globalPrototypeChainIsMutable())
  __proto__ = Proxy.create(this, "");

throw new InternalError("fallback");
