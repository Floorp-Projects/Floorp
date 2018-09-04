// |jit-test| error: TypeError
var x = newGlobal({sameCompartmentAs: this});
x instanceof x.Map.prototype.set;
