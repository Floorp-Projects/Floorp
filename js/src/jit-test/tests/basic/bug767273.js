var prox = Proxy.create({
  getPropertyDescriptor: function() { return undefined; },
  has:                   function() { return true; },
});

// Don't crash.
newGlobal("new-compartment").__lookupSetter__.call(prox, "e");
