// |jit-test| error: uncaught exception
var prox = new Proxy({}, {getOwnPropertyDescriptor: function() { throw prox; }});
Object.prototype.__lookupGetter__.call(prox, 'q');
