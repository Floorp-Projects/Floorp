// |jit-test| error: uncaught exception
var prox = Proxy.create({getOwnPropertyDescriptor: function() { throw prox; }});
Object.prototype.__lookupGetter__.call(prox, 'q');
