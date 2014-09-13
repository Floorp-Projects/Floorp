// |jit-test| error: uncaught exception
var prox = Proxy.create({getPropertyDescriptor: function() { throw prox; }});
Object.prototype.__lookupGetter__.call(prox, 'q');
