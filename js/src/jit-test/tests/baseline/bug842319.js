// |jit-test| error: TypeError
Array.prototype.__proto__ = Proxy.create({
    getPropertyDescriptor: function(name) {
	return 0;
    },
}, null);
var statusitems = [];
statusitems[0] = '';
