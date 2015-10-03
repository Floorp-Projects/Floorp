// |jit-test| error: m is not defined
load(libdir + "immutable-prototype.js");

if (globalPrototypeChainIsMutable()) {
  this.__proto__ = Proxy.create({
    has:function(){
      try {
        aa0 = Function(undefined);
      } catch (aa) {}
    }
  });
}
m();
