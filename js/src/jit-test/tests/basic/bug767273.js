var prox = Proxy.create({
  getOwnPropertyDescriptor: function() { return undefined; },
  has:                      function() { return true; },
});

// Don't crash.
newGlobal().__lookupSetter__.call(prox, "e");
