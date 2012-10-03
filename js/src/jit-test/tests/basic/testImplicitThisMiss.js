// |jit-test| error:TypeError
Function("Object.defineProperty(this, 'x', { configurable:true, get:function() { delete this['x'] } }); x()")();
