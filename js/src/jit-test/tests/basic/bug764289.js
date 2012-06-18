// Don't crash. May require valgrind.
var prox = Proxy.create({getPropertyDescriptor: function() { }});
Object.prototype.__lookupGetter__.call(prox, 'q');
