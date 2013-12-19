// |jit-test| error: InternalError

Array.prototype.__proto__ = Proxy.create({
    getPropertyDescriptor: function(name) {
	return (560566);
    },
}, null);
function f() {}
function g() {  }    
var x = [f,f,f,undefined,g];
for (var i = 0; i < 5; ++i)
  y = x[i]("x");
