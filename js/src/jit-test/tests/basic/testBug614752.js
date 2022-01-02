// |jit-test| error: TypeError
Object.prototype.apply = Function.prototype.apply;
({}).apply(null, null);                         // don't assert
